#include <fstream>
#include <iostream>
#include <string.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <queue>
#include <deque>
#include <vector>
#include <getopt.h>
#include <climits>

using namespace std;

char inBuffer[1048576];
int req_num = 0;
int CURRENT_TIME = 0;
int CURRENT_TRACK = 0;

struct Request {
    int rid;
    int arrival_time;
    int track;                      //track accessed
    int distance;                   //distance between track accessed and the current track
    int disk_start;                 //disk start time
    int disk_end;                   //disk end time
};

Request* CURRENT_RUNNING_IO;
deque<Request*> reqs;               //all IO requests from input file
deque<Request*> add_queue;          //all pending IO requests     
vector<Request*> req_summary;        //used to print info

class schBase {
    public:
        //virtual function
        virtual bool check_pending_request() = 0;
        virtual void add_io(Request* r) = 0;
        virtual Request* strategy() = 0;
};

schBase* THE_SCHEDULER;

class FIFO: public schBase {
    public:
        bool check_pending_request() {
            if (!add_queue.empty()) return true;
            return false;
        }

        void add_io(Request* r) {
            add_queue.push_back(r);
            reqs.pop_front();
        }

        Request* strategy() {
            Request* r = add_queue.front();
            r->disk_start = CURRENT_TIME;
            r->distance = abs(CURRENT_TRACK - r->track);
            r->disk_end = CURRENT_TIME + r->distance;
            add_queue.pop_front();
            return r;
        }
};

class SSTF: public schBase {
    private:
        Request* closest_r;
        int min_distance;
        int ind = 0;

    public:
        bool check_pending_request() {
            if (!add_queue.empty()) return true;
            return false;
        }
        
        void add_io(Request* r) {
            add_queue.push_back(r);
            reqs.pop_front();
        }

        Request* strategy() {
            min_distance = INT_MAX;
            for (int i = 0; i < add_queue.size(); i++) {
                Request* r = add_queue[i];
                int distance = abs(CURRENT_TRACK - r->track);
                if (distance < min_distance) {
                    min_distance = distance;
                    closest_r = r;
                    ind = i;
                }
            }
            closest_r->disk_start = CURRENT_TIME;
            closest_r->distance = min_distance;
            closest_r->disk_end = CURRENT_TIME + closest_r->distance;
            add_queue.erase(add_queue.begin() + ind);
            return closest_r;
        }
};

class LOOK: public schBase {
    private:
        Request* closest_r;
        int min_distance;
        int ind = 0;
        int dir = 1;

        Request* get_closest_r() {
            for (int i = 0; i < add_queue.size(); i++) {
                Request* r = add_queue[i];
                int distance = abs(CURRENT_TRACK - r->track);
                //up direction
                if (dir == 1 && r->track >= CURRENT_TRACK) {
                    if (distance < min_distance) {
                        min_distance = distance;
                        closest_r = r;
                        ind = i;
                    }
                }
                //down direction
                else if (dir == -1 && r->track <= CURRENT_TRACK) {
                    if (distance < min_distance) {
                        min_distance = distance;
                        closest_r = r;
                        ind = i;
                    }
                }
            }
            if (closest_r != nullptr) {
                closest_r->disk_start = CURRENT_TIME;
                closest_r->distance = min_distance;
                closest_r->disk_end = CURRENT_TIME + closest_r->distance;
                add_queue.erase(add_queue.begin() + ind);
                return closest_r;
            }
            return closest_r;
        }

    public:
        bool check_pending_request() {
            if (!add_queue.empty()) return true;
            return false;
        }
        
        void add_io(Request* r) {
            add_queue.push_back(r);
            reqs.pop_front();
        }

        Request* strategy() {
            closest_r = nullptr;
            min_distance = INT_MAX;
            if (get_closest_r() != nullptr) return closest_r;

            //change direction
            dir = dir * -1;
            min_distance = INT_MAX;
            return get_closest_r();

        }
};

class CLOOK: public schBase {
    private:
        Request* closest_r;
        int min_distance;
        int ind = 0;
        int lowest_ind;
        int lowest_track;

    public:
        bool check_pending_request() {
            if (!add_queue.empty()) return true;
            return false;
        }

        void add_io(Request* r) {
            add_queue.push_back(r);
            reqs.pop_front();
        }

        Request* strategy() {
            closest_r = nullptr;
            min_distance = INT_MAX;
            lowest_ind = INT_MAX;
            lowest_track = INT_MAX;
            for (int i = 0; i < add_queue.size(); i++) {
                Request* r = add_queue[i];
                int distance = abs(CURRENT_TRACK - r->track);
                if (r->track >= CURRENT_TRACK) {
                    if (distance < min_distance) {
                        min_distance = distance;
                        closest_r = r;
                        ind = i;
                    }
                }
                if (r->track < lowest_track) {
                    lowest_ind = i;
                    lowest_track = r->track;
                }
            }
            if (closest_r != nullptr) {
                closest_r->disk_start = CURRENT_TIME;
                closest_r->distance = min_distance;
                closest_r->disk_end = CURRENT_TIME + closest_r->distance;
                add_queue.erase(add_queue.begin() + ind);
                return closest_r;
            }
            //choose the lowest track
            Request* lowest_r = add_queue[lowest_ind];
            lowest_r->disk_start = CURRENT_TIME;
            lowest_r->distance = abs(CURRENT_TRACK - lowest_r->track);
            lowest_r->disk_end = CURRENT_TIME + lowest_r->distance;
            add_queue.erase(add_queue.begin() + lowest_ind);
            return lowest_r;
        }
};

class FLOOK: public schBase {
    private:
        deque<Request*> *queues;
        Request* closest_r;
        int min_distance;
        int ind = 0;
        int dir = 1;
        int AQ = 1;

        bool direction_check() {
            for (int i = 0; i < queues[AQ].size(); i++) {
                Request* r = queues[AQ][i];
                if (dir == 1 && r->track >= CURRENT_TRACK) {
                    return false;
                }
                if (dir == -1 && r->track <= CURRENT_TRACK) {
                    return false;
                }
            }
            return true;
        }

    public:
        //constructer
        FLOOK() { queues = new deque<Request*>[2]; }

        bool check_pending_request() {
            if (queues[1-AQ].empty() && queues[AQ].empty()) return false;
            return true;
        }

        void add_io(Request* r) {
            queues[1-AQ].push_back(r);
            reqs.pop_front();
        }

        Request* strategy() {
            //swap active_queue and add_queue if active_queue is empty
            if (queues[AQ].empty()) {
                AQ = 1 - AQ;
            }
            //if needed change direction
            bool change = direction_check();
            if (change) {
                dir = dir * -1;
            }

            closest_r = nullptr;
            min_distance = INT_MAX;
            for (int i = 0; i < queues[AQ].size(); i++) {
                Request* r = queues[AQ][i];
                int distance = r->track - CURRENT_TRACK;
                //up direction
                if (dir == 1 && distance >= 0) {
                    if (distance < min_distance) {
                        min_distance = distance;
                        closest_r = r;
                        ind = i;
                    }
                }
                //down direction
                else if (dir == -1 && distance <= 0) {
                    if (abs(distance) < min_distance) {
                        min_distance = abs(distance);
                        closest_r = r;
                        ind = i;
                    }
                }
            }
            closest_r->disk_start = CURRENT_TIME;
            closest_r->distance = min_distance;
            closest_r->disk_end = CURRENT_TIME + closest_r->distance;
            queues[AQ].erase(queues[AQ].begin() + ind);
            return closest_r;
        }
};

int get_new_io_time() {
    if (reqs.empty()) return -1;
    return reqs.front()->arrival_time;
}

void Simulation() {
    while (true) {
        if (get_new_io_time() == CURRENT_TIME) {
            THE_SCHEDULER->add_io(reqs.front());
        }
        if (CURRENT_RUNNING_IO && CURRENT_RUNNING_IO->disk_end == CURRENT_TIME) {
            Request* r = CURRENT_RUNNING_IO;
            req_summary[r->rid] = r;
            CURRENT_RUNNING_IO = nullptr;
        }
        if (CURRENT_RUNNING_IO == nullptr) {
            if (THE_SCHEDULER->check_pending_request()) {
                CURRENT_RUNNING_IO = THE_SCHEDULER->strategy();
                continue;
            }
            else if (reqs.empty()) {
                break;
            }
        }
        if (CURRENT_RUNNING_IO) {
            if (CURRENT_RUNNING_IO->track > CURRENT_TRACK) CURRENT_TRACK++;
            else CURRENT_TRACK--;
        }
        CURRENT_TIME++;
    }
}

void print_sch_info() {
    int total_time = 0;
    int total_movement = 0;
    double total_turnaround = 0;
    double total_waittime = 0;
    int max_waittime = 0;

    for (int i = 0; i < req_summary.size(); i++) {
        Request* r = req_summary[i];
        total_time = max(total_time, r->disk_end);
        total_movement += r->disk_end - r->disk_start;
        total_turnaround += r->disk_end - r->arrival_time;
        total_waittime += r->disk_start - r->arrival_time;
        max_waittime = max(max_waittime, (r->disk_start - r->arrival_time));
        printf("%5d: %5d %5d %5d\n", i, r->arrival_time, r->disk_start, r->disk_end);
    }
    printf("SUM: %d %d %.2lf %.2lf %d\n"
            , total_time, total_movement, total_turnaround / req_num, total_waittime / req_num, max_waittime);
}

void read_line(ifstream& inFile) {
    do {
        inFile.getline(inBuffer, 1048576);
    } while (inBuffer[0] == '#');
}

int main(int argc, char* argv[]) {
    int arg;
    char algo;
    setbuf(stdout, NULL);

    while ((arg = getopt(argc, argv, "s:")) != -1) {
        switch (arg) {
        case 's':
            //Scheduler Algorithms
            algo = tolower(*optarg);
            switch (algo) {
            case 'i':
                THE_SCHEDULER = new FIFO();
                break;
            
            case 'j':
                THE_SCHEDULER = new SSTF();
                break;
            
            case 's':
                THE_SCHEDULER = new LOOK();
                break;
            
            case 'c':
                THE_SCHEDULER = new CLOOK();
                break;
            
            case 'f':
                THE_SCHEDULER = new FLOOK();
                break;
            }
            break;
        }
    }

    /*********read input file*********/
    ifstream inFile(argv[optind]);
    int rid = 0, AT = 0, TRACK = 0;

    read_line(inFile);
    while (!inFile.eof()) {
        sscanf(inBuffer, "%d %d", &AT, &TRACK);
        reqs.push_back( new Request {rid++, AT, TRACK, 0, 0, 0} );
        read_line(inFile);
    }
    req_num = reqs.size();
    req_summary.resize(req_num);
    /*********************************/

    Simulation();
    print_sch_info();
    return 0;
}