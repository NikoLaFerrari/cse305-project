#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <queue>
#include <limits>
#include <functional>

namespace fs = std::filesystem;
using hrclock = std::chrono::high_resolution_clock;
using us      = std::chrono::microseconds;

// ============================================================
// POINT2D
// ============================================================
struct Point2D {
    double x, y;
    Point2D(double x=0, double y=0) : x(x), y(y) {}
};
inline double sq_dist(const Point2D& a, const Point2D& b) {
    double dx=a.x-b.x, dy=a.y-b.y; return dx*dx+dy*dy;
}

// ============================================================
// ARFF LOADER
// ============================================================
std::vector<Point2D> load_arff(const std::string& path) {
    std::vector<Point2D> pts;
    std::ifstream f(path);
    if (!f.is_open()) return pts;
    std::string line; bool in_data=false;
    while (std::getline(f,line)) {
        if (line.empty()||line[0]=='%') continue;
        std::string lower=line;
        std::transform(lower.begin(),lower.end(),lower.begin(),::tolower);
        if (lower.find("@data")!=std::string::npos){in_data=true;continue;}
        if (!in_data) continue;
        if (!line.empty()&&line.back()=='\r') line.pop_back();
        if (line.empty()||line[0]=='%') continue;
        std::stringstream ss(line); std::string t1,t2;
        if (std::getline(ss,t1,',')&&std::getline(ss,t2,',')) {
            try { pts.push_back({std::stod(t1),std::stod(t2)}); }
            catch(...) { continue; }
        }
    }
    return pts;
}


struct Result {
    double left_x=0,left_y=0,right_x=0,right_y=0;
    int left_count=0,right_count=0;
    long long time_us=0; bool valid=false;
};
bool results_match(const Result& a, const Result& b, double eps=1e-3) {
    if (!a.valid||!b.valid) return false;
    auto close=[&](double x,double y){return std::abs(x-y)<eps;};
    return (close(a.left_x,b.left_x)&&close(a.left_y,b.left_y)&&
            close(a.right_x,b.right_x)&&close(a.right_y,b.right_y))
        || (close(a.left_x,b.right_x)&&close(a.left_y,b.right_y)&&
            close(a.right_x,b.left_x)&&close(a.right_y,b.left_y));
}


struct ClusterNode {
    int     id, sample_count;
    Point2D centroid;
    int     generation        = 0;
    int     left_id           = -1;
    int     right_id          = -1;
    int     active_view_index = -1;
    ClusterNode(int id, const Point2D& p)
        : id(id), sample_count(1), centroid(p) {}
    ClusterNode(int id, int lid, int rid, const Point2D& c, int cnt)
        : id(id), sample_count(cnt), centroid(c), left_id(lid), right_id(rid) {}
};


struct PQEntry {
    double dist;
    int    from_id, to_id, from_generation;
    bool operator>(const PQEntry& o) const {
        if (dist != o.dist) return dist > o.dist;
        int a0=std::min(from_id,to_id), a1=std::max(from_id,to_id);
        int b0=std::min(o.from_id,o.to_id), b1=std::max(o.from_id,o.to_id);
        if (a0!=b0) return a0>b0;
        return a1>b1;
    }
};
using MinHeap = std::priority_queue<PQEntry,std::vector<PQEntry>,std::greater<PQEntry>>;

// ============================================================
// THREAD POOL 
// ============================================================
struct ThreadPool {
    struct LocalBest { double dist=std::numeric_limits<double>::max(); int node_id=-1; };
    std::vector<LocalBest>           results;
    std::vector<std::thread>         workers;
    std::function<void(int,int,int)> task;
    int                              n_active=0;
    std::mutex                       mtx;
    std::condition_variable          cv_work, cv_done;
    int                              pending=0, round=0;
    bool                             stop=false;

    explicit ThreadPool(int T) : results(T) {
        workers.reserve(T);
        for (int t=0;t<T;++t) workers.emplace_back([this,t]{ loop(t); });
    }
    ~ThreadPool() {
        { std::lock_guard<std::mutex> lk(mtx); stop=true; ++round; }
        cv_work.notify_all();
        for (auto& w:workers) w.join();
    }
    int size() const { return (int)workers.size(); }
    void run(std::function<void(int,int,int)> fn, int n) {
        { std::lock_guard<std::mutex> lk(mtx); task=std::move(fn); n_active=n; pending=size(); ++round; }
        cv_work.notify_all();
        std::unique_lock<std::mutex> lk(mtx);
        cv_done.wait(lk,[this]{ return pending==0; });
    }
private:
    void loop(int t) {
        int last=0;
        while (true) {
            std::unique_lock<std::mutex> lk(mtx);
            cv_work.wait(lk,[&]{ return round!=last||stop; });
            last=round; if (stop) return;
            auto fn=task; int n=n_active; lk.unlock();
            int stripe=(n+size()-1)/size();
            int begin=t*stripe, end=std::min(begin+stripe,n);
            fn(t,begin,end);
            lk.lock();
            if (--pending==0) cv_done.notify_one();
        }
    }
};

// ============================================================
// SEQUENTIAL ENGINE
// ============================================================
class SequentialEngine {
    std::vector<ClusterNode*> arena, active;
    int sz=0, root_id=-1;
    MinHeap pq;

    void active_push(ClusterNode* n) {
        n->active_view_index=sz;
        if (sz<(int)active.size()) active[sz]=n; else active.push_back(n);
        ++sz;
    }
    void active_remove(ClusterNode* n) {
        int slot=n->active_view_index;
        ClusterNode* last=active[--sz];
        active[slot]=last; last->active_view_index=slot;
        n->active_view_index=-1;
    }
    ClusterNode* alloc_leaf(const Point2D& p) {
        int id=(int)arena.size(); auto* n=new ClusterNode(id,p); arena.push_back(n); return n;
    }
    ClusterNode* alloc_merge(int lid,int rid,const Point2D& c,int cnt) {
        int id=(int)arena.size(); auto* n=new ClusterNode(id,lid,rid,c,cnt); arena.push_back(n); return n;
    }
    void push_nearest(ClusterNode* n) {
        double best=std::numeric_limits<double>::max(); int best_id=-1;
        for (int i=0;i<sz;++i) {
            ClusterNode* c=active[i]; if(c==n) continue;
            double d=sq_dist(n->centroid,c->centroid);
            bool better=(d<best)||(d==best&&c->id<best_id);
            if(better){best=d;best_id=c->id;}
        }
        if(best_id>=0) pq.push({best,n->id,best_id,n->generation});
    }
    void merge_step() {
        while (!pq.empty()) {
            auto e=pq.top(); pq.pop();
            ClusterNode* from=arena[e.from_id]; ClusterNode* to=arena[e.to_id];
            if (e.from_generation!=from->generation) continue;
            if (from->active_view_index<0)           continue;
            if (to->active_view_index<0)             continue;
            int cnt=from->sample_count+to->sample_count;
            Point2D c{(from->centroid.x*from->sample_count+to->centroid.x*to->sample_count)/cnt,
                      (from->centroid.y*from->sample_count+to->centroid.y*to->sample_count)/cnt};
            ClusterNode* merged=alloc_merge(from->id,to->id,c,cnt);
            ++from->generation; ++to->generation;
            active_remove(from); active_remove(to);
            active_push(merged); push_nearest(merged);
            root_id=merged->id; return;
        }
    }
public:
    ~SequentialEngine() { for (auto* n:arena) delete n; }
    Result run(const std::vector<Point2D>& pts, bool progress=false) {
        for (auto* n:arena) delete n;
        arena.clear(); active.clear(); sz=0; root_id=-1;
        while (!pq.empty()) pq.pop();
        int N=(int)pts.size(); if(N<2) return {};
        arena.reserve(2*N-1); active.reserve(N);
        for (auto& p:pts) active_push(alloc_leaf(p));
        for (int i=0;i<sz;++i) push_nearest(active[i]);
        auto t0=hrclock::now();
        int merges=0,total=N-1,next_pct=10;
        while (sz>1) {
            merge_step();
            if (progress) {
                int pct=++merges*100/total;
                if (pct>=next_pct){std::cerr<<"\r  seq "<<std::setw(3)<<pct<<"%  "<<std::flush;next_pct=pct+10;}
            }
        }
        if (progress) std::cerr<<"\r                    \r"<<std::flush;
        auto t1=hrclock::now();
        Result r; r.time_us=std::chrono::duration_cast<us>(t1-t0).count(); r.valid=(root_id>=0);
        if (r.valid&&arena[root_id]->left_id>=0) {
            auto* L=arena[arena[root_id]->left_id]; auto* R=arena[arena[root_id]->right_id];
            r.left_x=L->centroid.x; r.left_y=L->centroid.y; r.left_count=L->sample_count;
            r.right_x=R->centroid.x; r.right_y=R->centroid.y; r.right_count=R->sample_count;
        }
        return r;
    }
};

// ============================================================
// PARALLEL ENGINE 
// ============================================================
class ParallelEngine {
    std::vector<ClusterNode*> arena, active;
    int sz=0, root_id=-1;
    MinHeap pq;
    ThreadPool pool;

    void active_push(ClusterNode* n) {
        n->active_view_index=sz;
        if (sz<(int)active.size()) active[sz]=n; else active.push_back(n);
        ++sz;
    }
    void active_remove(ClusterNode* n) {
        int slot=n->active_view_index;
        ClusterNode* last=active[--sz];
        active[slot]=last; last->active_view_index=slot;
        n->active_view_index=-1;
    }
    ClusterNode* alloc_leaf(const Point2D& p) {
        int id=(int)arena.size(); auto* n=new ClusterNode(id,p); arena.push_back(n); return n;
    }
    ClusterNode* alloc_merge(int lid,int rid,const Point2D& c,int cnt) {
        int id=(int)arena.size(); auto* n=new ClusterNode(id,lid,rid,c,cnt); arena.push_back(n); return n;
    }

    
    void seed_all() {
        int T=pool.size();
        std::vector<MinHeap> local(T);
        pool.run([&](int t,int begin,int end){
            for (int i=begin;i<end;++i){
                ClusterNode* node=active[i];
                double best=std::numeric_limits<double>::max(); int best_id=-1;
                for (int j=0;j<sz;++j){
                    ClusterNode* c=active[j]; if(c==node) continue;
                    double d=sq_dist(node->centroid,c->centroid);
                    bool better=(d<best)||(d==best&&c->id<best_id);
                    if(better){best=d;best_id=c->id;}
                }
                if(best_id>=0) local[t].push({best,node->id,best_id,node->generation});
            }
        }, sz);
        for (auto& h:local) while(!h.empty()){pq.push(h.top());h.pop();}
    }

    
    void parallel_push_nearest(ClusterNode* target) {
        pool.run([&,target](int t,int begin,int end){
            double best=std::numeric_limits<double>::max(); int best_id=-1;
            for (int i=begin;i<end;++i){
                ClusterNode* c=active[i]; if(c==target) continue;
                double d=sq_dist(target->centroid,c->centroid);
                bool better=(d<best)||(d==best&&c->id<best_id);
                if(better){best=d;best_id=c->id;}
            }
            pool.results[t]={best,best_id};
        }, sz);
        double best=std::numeric_limits<double>::max(); int best_id=-1;
        for (auto& r:pool.results){
            bool better=(r.dist<best)||(r.dist==best&&r.node_id>=0&&r.node_id<best_id);
            if(r.node_id>=0&&better){best=r.dist;best_id=r.node_id;}
        }
        if(best_id>=0) pq.push({best,target->id,best_id,target->generation});
    }

    void merge_step() {
        while (!pq.empty()) {
            auto e=pq.top(); pq.pop();
            ClusterNode* from=arena[e.from_id]; ClusterNode* to=arena[e.to_id];
            if (e.from_generation!=from->generation) continue;
            if (from->active_view_index<0)           continue;
            if (to->active_view_index<0)             continue;
            int cnt=from->sample_count+to->sample_count;
            Point2D c{(from->centroid.x*from->sample_count+to->centroid.x*to->sample_count)/cnt,
                      (from->centroid.y*from->sample_count+to->centroid.y*to->sample_count)/cnt};
            ClusterNode* merged=alloc_merge(from->id,to->id,c,cnt);
            ++from->generation; ++to->generation;
            active_remove(from); active_remove(to);
            active_push(merged);
            parallel_push_nearest(merged);
            root_id=merged->id; return;
        }
    }

public:
    explicit ParallelEngine(int threads) : pool(threads) {}
    ~ParallelEngine() { for (auto* n:arena) delete n; }

    Result run(const std::vector<Point2D>& pts, bool progress=false, const char* tag="par") {
        for (auto* n:arena) delete n;
        arena.clear(); active.clear(); sz=0; root_id=-1;
        while (!pq.empty()) pq.pop();
        int N=(int)pts.size(); if(N<2) return {};
        arena.reserve(2*N-1); active.reserve(N);
        for (auto& p:pts) active_push(alloc_leaf(p));
        seed_all();
        auto t0=hrclock::now();
        int merges=0,total=N-1,next_pct=10;
        while (sz>1) {
            merge_step();
            if (progress) {
                int pct=++merges*100/total;
                if (pct>=next_pct){std::cerr<<"\r  "<<tag<<" "<<std::setw(3)<<pct<<"%  "<<std::flush;next_pct=pct+10;}
            }
        }
        if (progress) std::cerr<<"\r                    \r"<<std::flush;
        auto t1=hrclock::now();
        Result r; r.time_us=std::chrono::duration_cast<us>(t1-t0).count(); r.valid=(root_id>=0);
        if (r.valid&&arena[root_id]->left_id>=0) {
            auto* L=arena[arena[root_id]->left_id]; auto* R=arena[arena[root_id]->right_id];
            r.left_x=L->centroid.x; r.left_y=L->centroid.y; r.left_count=L->sample_count;
            r.right_x=R->centroid.x; r.right_y=R->centroid.y; r.right_count=R->sample_count;
        }
        return r;
    }
};

// ============================================================
// BENCHMARK
// ============================================================
void print_header() {
    std::cout<<"\n"<<std::string(129,'=')<<"\n";
    std::cout<<std::left
             <<std::setw(35)<<"File"
             <<std::setw(7) <<"N"
             <<std::setw(14)<<"Seq(ms)"
             <<std::setw(14)<<"Par-1(ms)"
             <<std::setw(14)<<"Par-2(ms)"
             <<std::setw(14)<<"Par-4(ms)"
             <<std::setw(14)<<"Par-8(ms)"
             <<std::setw(10)<<"Speedup-4"
             <<std::setw(8) <<"Match"
             <<"\n"<<std::string(129,'-')<<"\n";
}

void run_file(const std::string& path, const std::string& shortname,
              SequentialEngine& seq, ParallelEngine& p1, ParallelEngine& p2, ParallelEngine& p4, ParallelEngine& p8) {
    auto pts=load_arff(path);
    if (pts.size()<10) return;

    bool large=pts.size()>5000;
    if (large) {
        std::cout<<std::left<<std::setw(35)<<shortname
                 <<std::setw(7)<<pts.size()<<" running..."<<std::flush;
    }

    Result s =seq.run(pts,large);
    Result r1=p1.run(pts,large,"p1");
    Result r2=p2.run(pts,large,"p2");
    Result r4=p4.run(pts,large,"p4");
    Result r8=p8.run(pts,large,"p8");

    bool match=results_match(s,r1)&&results_match(s,r2)&&results_match(s,r4)&&results_match(s,r8);
    double speedup4=(r4.time_us>0)?(double)s.time_us/r4.time_us:0;

    if (large) std::cout<<"\r";
    std::cout<<std::left
             <<std::setw(35)<<shortname
             <<std::setw(7) <<pts.size()
             <<std::setw(14)<<std::fixed<<std::setprecision(2)<<s.time_us/1000.0
             <<std::setw(14)<<r1.time_us/1000.0
             <<std::setw(14)<<r2.time_us/1000.0
             <<std::setw(14)<<r4.time_us/1000.0
             <<std::setw(14)<<r8.time_us/1000.0
             <<std::setw(10)<<std::setprecision(2)<<speedup4<<"x"
             <<std::setw(8) <<(match?"YES":"NO!!!")<<"\n";

    if (!match) {
        std::cout<<"  SEQ: L=("<<s.left_x<<","<<s.left_y<<") R=("<<s.right_x<<","<<s.right_y<<")\n";
        std::cout<<"  P4:  L=("<<r4.left_x<<","<<r4.left_y<<") R=("<<r4.right_x<<","<<r4.right_y<<")\n";
    }
}

int main(int argc, char** argv) {
    std::string base=(argc>1)?argv[1]:"dataset";
    std::cout<<"Hierarchical Clustering (PQ) — Dataset Benchmark\n";
    std::cout<<"Hardware threads: "<<std::thread::hardware_concurrency()<<"\n";
    std::cout<<"Dataset root: "<<base<<"\n";

    SequentialEngine seq;
    ParallelEngine p1(1), p2(2), p4(4), p8(8);

    std::vector<std::string> targets={
        "synthetic/2d-10c.arff","synthetic/2d-4c.arff","synthetic/aggregation.arff",
        "synthetic/banana.arff","synthetic/cassini.arff","synthetic/compound.arff",
        "synthetic/cure-t1-2000n-2D.arff","synthetic/cure-t2-4k.arff","synthetic/D31.arff",
        "synthetic/diamond9.arff","synthetic/disk-1000n.arff","synthetic/disk-3000n.arff",
        "synthetic/flame.arff","synthetic/hepta.arff","synthetic/jain.arff",
        "synthetic/lsun.arff","synthetic/pathbased.arff","synthetic/R15.arff",
        "synthetic/rings.arff","synthetic/s-set1.arff","synthetic/s-set2.arff",
        "synthetic/shapes.arff","synthetic/smile2.arff","synthetic/spiral.arff",
        "synthetic/tetra.arff","synthetic/zelnik1.arff",
        "UCI/iris.arff","UCI/wine.arff","UCI/glass.arff","UCI/ecoli.arff",
        "UCI/vehicle.arff","UCI/segment.arff","UCI/wdbc.arff",
        "UCI/sonar.arff","UCI/ionosphere.arff",
    };

    std::string cur_dir;
    print_header();
    for (auto& rel:targets) {
        std::string dir=rel.substr(0,rel.find('/'));
        if (dir!=cur_dir){if(!cur_dir.empty())std::cout<<"\n";std::cout<<"  --- "<<dir<<" ---\n";cur_dir=dir;}
        std::string full=base+"/"+rel;
        std::string shortname=rel.substr(rel.find('/')+1);
        if (shortname.size()>5) shortname=shortname.substr(0,shortname.size()-5);
        run_file(full,shortname,seq,p1,p2,p4,p8);
    }

    std::cout<<"\n--- Additional synthetic files ---\n";
    std::vector<std::string> scanned;
    std::error_code ec;
    for (auto& entry:fs::directory_iterator(base+"/synthetic",ec)) {
        if (entry.path().extension()==".arff") {
            std::string name=entry.path().filename().string();
            bool already=false;
            for (auto& t:targets) if(t.find(name)!=std::string::npos){already=true;break;}
            if (!already) scanned.push_back(entry.path().string());
        }
    }
    std::sort(scanned.begin(),scanned.end());
    for (auto& p:scanned){
        std::string shortname=fs::path(p).stem().string();
        run_file(p,shortname,seq,p1,p2,p4,p8);
    }

    std::cout<<"\n"<<std::string(129,'=')<<"\n"<<"Benchmark complete.\n";
    return 0;
}