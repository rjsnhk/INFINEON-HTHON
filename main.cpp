#include <bits/stdc++.h>
using namespace std;

class Clan {
public:
    string name;
    bool isMine;
    int MAR;                // maximum availabl resource
    int PTR;                // processing time per resource
    int RT;                 // refill time
    int currentAvailable;   // cur available resource
    bool busy;              // true jab mine extraction mein busy ho
    double extractionStartTime; // extraction kab start hui
    int extractionRequest;      // kitna resource extract karna hai

    // For blocking
    bool blocked;             // true agar clan block ho
    double blockedUntil;      // block kab tak chalega
    bool firstRequestDuringBlock; // block ke time pehli extraction request ke liye flag

    // while extraction
    double effectivePTR;      // actual processing time per resource (adjusted during block)
    double ongoingGCO;        // current attack ke liye gco

    Clan(string _name, bool _isMine, int _MAR, int _PTR, int _RT) {
        name = _name;
        isMine = _isMine;
        MAR = _MAR;
        PTR = _PTR;
        RT = _RT;
        busy = false;
        currentAvailable = _MAR;
        extractionStartTime = 0;
        extractionRequest = 0;
        // Initially, koi block nahi hai
        blocked = false;
        blockedUntil = 0;
        firstRequestDuringBlock = false;
        effectivePTR = PTR; // initially normal speed
        ongoingGCO = 0;
    }
};

enum EventType {
    TROOP_ARRIVAL,       // troops arrive at the supplier mine and begin extraction
    EXTRACTION_COMPLETE, // mining completes at the supplier mine
    TROOP_RETURN,        // troops return with resources
    MINE_REFILL,         // mine is refilled after RT seconds 
    BLOCK,               // agar kisi clan ko block krte h
    UNBLOCK              // clan ko unblock krne k liye
};

struct AttackRequest {
    string targetClan;   // jis clan p attack shuru
    int required;        // resources required to defend (RR)
    double gco;          // gold capturing opportunity (agar attack pura hua)
    string supplierMine; // konsi mine se resource supply hogi
};

struct Event {
    double time;
    EventType type;
    AttackRequest attack;  // used for troop arrival, extraction and return events
    string mineName;       /// used for mine refill events, block unblock
    int blockDuration;     // block kitni der

    // comparison for min-heap priority queue (earlier time sabse pehle)
    bool operator>(const Event& other) const {
        return time > other.time;
    }
};

class Simulation {
private:
    double currentTime;
    double goldCaptured;
    unordered_map<string, Clan> clans;  // map clan name to clan data
    unordered_map<string, vector<pair<string, int>>> graph;// for each clan, list of (neighbor, travel time)
    // event queue as a min–heap sorted by event time

    priority_queue<Event, vector<Event>, greater<Event>> eventQueue;
    // Slowdown
    const double slowdownFactor = 0.7;

public:
    Simulation() : currentTime(0), goldCaptured(0) {}

    // add clan to the simulation
    void addClan(const Clan& clan) {
        clans.insert({ clan.name, clan });
    }

    // Road add karo (bidirectional)
    void addRoad(const string& from, const string& to, int travelTime) {
        graph[from].push_back({ to, travelTime });
        graph[to].push_back({ from, travelTime });
    }

    // Dijkstras to calc travel time btw clans
    int shortestTravelTime(const string& start, const string& end) {
        unordered_map<string, int> dist;
        for (const auto& p : clans)
            dist[p.first] = 1e9; // infinity cause sb phele door h
        typedef pair<int, string> P;
        priority_queue<P, vector<P>, greater<P>> pq;
        dist[start] = 0;
        pq.push({ 0, start });
        while (!pq.empty()) {
            auto d = pq.top().first;
            auto u = pq.top().second;
            pq.pop();
            if (d > dist[u])
                continue;
            if (u == end)
                return d;
            for (auto& edge : graph[u]) {
                string v = edge.first;
                int w = edge.second;
                if (dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w;
                    pq.push({ dist[v], v });
                }
            }
        }
        return 1e9;
    }

    // schedule a new event
    void scheduleEvent(const Event& e) {
        eventQueue.push(e);
    }

    // Process all events scheduled jbtk nai query nhi aati also same for ending query
    void processEvents(double nextQueryTime) {
        while (!eventQueue.empty() && eventQueue.top().time <= nextQueryTime) {
            Event e = eventQueue.top();
            eventQueue.pop();
            currentTime = e.time;
            switch (e.type) {
            case TROOP_ARRIVAL: {
                // Troops supplier mine pe phonch gaye, extraction start krdo
                Clan& supplier = clans.at(e.attack.supplierMine);
                supplier.busy = true;
                supplier.extractionStartTime = currentTime;
                supplier.extractionRequest = e.attack.required;
                supplier.ongoingGCO = e.attack.gco;
                double extractionCompleteTime = 0;
                // block case
                if (supplier.blocked && currentTime < supplier.blockedUntil) {
                    if (!supplier.firstRequestDuringBlock) {
                        supplier.firstRequestDuringBlock = true;
                        supplier.effectivePTR = supplier.PTR; // pehli request normal speed se
                        extractionCompleteTime = currentTime + supplier.extractionRequest * supplier.effectivePTR;
                    }
                    else {
                        // Subsequent requests: extraction time accelerated by factor 0.7
                        supplier.effectivePTR = supplier.PTR * slowdownFactor;
                        extractionCompleteTime = currentTime + supplier.extractionRequest * supplier.effectivePTR;
                    }
                }
                else {
                    supplier.effectivePTR = supplier.PTR;
                    extractionCompleteTime = currentTime + supplier.extractionRequest * supplier.effectivePTR;
                }
                // kaam 25
                Event extractionComplete;
                extractionComplete.time = extractionCompleteTime;
                extractionComplete.type = EXTRACTION_COMPLETE;
                extractionComplete.attack = e.attack;
                scheduleEvent(extractionComplete);
                break;
            }
            case EXTRACTION_COMPLETE: {
                // extraction poora hogya, update the suppliers available resources
                Clan& supplier = clans.at(e.attack.supplierMine);
                supplier.currentAvailable -= e.attack.required;
                supplier.busy = false;
                // schedule troop return (travel time same as from target to supplier)
                int travelTime = shortestTravelTime(e.attack.targetClan, supplier.name);
                double troopReturnTime = currentTime + travelTime;
                Event troopReturn;
                troopReturn.time = troopReturnTime;
                troopReturn.type = TROOP_RETURN;
                troopReturn.attack = e.attack;
                scheduleEvent(troopReturn);
                // schedule mine refill event RT secs k baad
                Event refill;
                refill.time = currentTime + supplier.RT;
                refill.type = MINE_REFILL;
                refill.mineName = supplier.name;
                scheduleEvent(refill);
                break;
            }
            case TROOP_RETURN: {
                // Troops wapas aagaye, add gold
                goldCaptured += e.attack.gco;
                break;
            }
            case MINE_REFILL: {
                // refill mine  
                Clan& mine = clans.at(e.mineName);
                mine.currentAvailable = mine.MAR;
                mine.busy = false;
                break;
            }
            case BLOCK: {
                //clan ko block mark kardo 
                Clan& c = clans.at(e.mineName);
                c.blocked = true;
                c.blockedUntil = currentTime + e.blockDuration;
                c.firstRequestDuringBlock = false; // new block period
                break;
            }
            case UNBLOCK: {
                //block khatam, unblock
                Clan& c = clans.at(e.mineName);
                c.blocked = false;
                c.firstRequestDuringBlock = false;
                break;
            }
            }
        }
        currentTime = nextQueryTime;
    }

    // process attack at time queryTime on  targetclan with given RR and GCO
    void processAttack(double queryTime, const string& targetClan, int RR, double GCO) {
        processEvents(queryTime);
        // Choose the best available supplier mine for attack
        string bestMine;
        double bestTime = 1e9;
        for (auto& p : clans) {
            if (p.second.isMine) {
                int travelTime = shortestTravelTime(targetClan, p.first);
                if (travelTime == 1e9)
                    continue; // connected nahi
                if (p.second.busy)
                    continue; // mine busy hai
                double totalTime = travelTime + RR * p.second.PTR + travelTime;
                if (totalTime < bestTime) {
                    bestTime = totalTime;
                    bestMine = p.first;
                }
            }
        }
        if (bestMine == "")
            return;
        AttackRequest req;
        req.targetClan = targetClan;
        req.required = RR;
        req.gco = GCO;
        req.supplierMine = bestMine;
        int travelTime = shortestTravelTime(targetClan, bestMine);
        Event troopArrival;
        troopArrival.time = queryTime + travelTime;
        troopArrival.type = TROOP_ARRIVAL;
        troopArrival.attack = req;
        scheduleEvent(troopArrival);
    }

    // b=lock process query 
    void processBlockQuery(double queryTime, const string& clanName, int blockDuration) {
        processEvents(queryTime);
        Event blockEvent;
        blockEvent.time = queryTime;
        blockEvent.type = BLOCK;
        blockEvent.mineName = clanName;
        blockEvent.blockDuration = blockDuration;
        scheduleEvent(blockEvent);
        // Uublock event bhi krna pdega after blockage
        Event unblockEvent;
        unblockEvent.time = queryTime + blockDuration;
        unblockEvent.type = UNBLOCK;
        unblockEvent.mineName = clanName;
        scheduleEvent(unblockEvent);
    }

    // abhi hr mine m kitna bcha h
    void processStatusQuery(double queryTime) {
        processEvents(queryTime);
        vector<Clan*> mineClans;
        for (auto& p : clans) {
            if (p.second.isMine)
                mineClans.push_back(&p.second);
        }
        sort(mineClans.begin(), mineClans.end(), [](const Clan* a, const Clan* b) {
            return a->MAR > b->MAR;
            });
        for (auto* mine : mineClans) {
            int available;
            if (mine->busy) {
                double elapsed = currentTime - mine->extractionStartTime;
                int extracted = min(mine->extractionRequest, (int)(elapsed / mine->effectivePTR));
                available = mine->MAR - extracted;
            }
            else {
                available = mine->currentAvailable;
            }
            cout << mine->name << ": " << available << "/" << mine->MAR << " available ";
        }
        cout << endl;
    }

    // print godl for cur time
    void processGoldQuery(double queryTime) {
        processEvents(queryTime);
        cout << "Gold captured: " << goldCaptured << endl;
    }
};
int main() {
    // ----- Test Case 1 -----
    // XML Input:
    // clan_a: True, MAR=20, RT=5, PTR=1
    // clan_b: True, MAR=10, RT=1, PTR=2
    // clan_c: False
    // Road: clan_c to clan_a, Time=10
    // Queries: Process inputs, Attack on clan_c with 5 RR & 10 GCO, status at 2, status at 14, gold at 27, Victory at 30
    {
        Simulation sim;
        sim.addClan(Clan("clan_a", true, 20, 1, 5));
        sim.addClan(Clan("clan_b", true, 10, 2, 1));
        sim.addClan(Clan("clan_c", false, 0, 0, 0));
        sim.addRoad("clan_c", "clan_a", 10);

        sim.processAttack(1, "clan_c", 5, 10.0);
        sim.processStatusQuery(2);   // Expected: "clan_a: 20/20 available clan_b: 10/10 available"
        sim.processStatusQuery(14);  // Expected: "clan_a: 17/20 available clan_b: 10/10 available"
        sim.processGoldQuery(27);    // Expected: "Gold captured: 10"
        sim.processEvents(30);       // Victory of Codeopia
    }

    cout << "--------------------------------\n";

    // ----- Test Case 2 -----
    // XML Input:
    // clan_a: True, MAR=200, RT=120, PTR=1
    // clan_b: False
    // clan_c: False
    // Roads: clan_c to clan_a (20), clan_b to clan_a (15)
    // Queries:
    // 0: Process inputs
    // 1: Attack on clan_b with 30 RR & 15 GCO  -> Troop arrival at 1+15 = 16; first extraction normal; extraction complete at 16+30=46; troop return at 46+15=61; awards 15 GCO.
    // 6: Attack on clan_c with 70 RR & 35 GCO  -> Troop arrival at 6+20 = 26; second extraction during block, effective PTR = 1*0.7 = 0.7; extraction complete at 26+70*0.7=26+49=75; troop return at 75+20=95; awards 35 GCO.
    // 11: Block clan_a for 30 sec (block from 11 to 41)
    // 30: Gold captured query -> Expected: 0 (troop return for first attack not yet processed if partial gold is ignored)
    // 61: Gold captured query -> Expected: 15
    // 95: Gold captured query -> Expected: 50 (15+35)
    // 100: Victory of Codeopia
    {
        Simulation sim;
        sim.addClan(Clan("clan_a", true, 200, 1, 120));
        sim.addClan(Clan("clan_b", false, 0, 0, 0));
        sim.addClan(Clan("clan_c", false, 0, 0, 0));
        sim.addRoad("clan_c", "clan_a", 20);
        sim.addRoad("clan_b", "clan_a", 15);

        sim.processAttack(1, "clan_b", 30, 15.0);
        sim.processAttack(6, "clan_c", 70, 35.0);
        sim.processBlockQuery(11, "clan_a", 30);
        sim.processGoldQuery(30);  // Expected: "Gold captured: 0"
        sim.processGoldQuery(61);  // Expected: "Gold captured: 15"
        sim.processGoldQuery(95);  // Expected: "Gold captured: 50"
        sim.processEvents(100);
    }

    cout << "--------------------------------\n";

    // ----- Test Case 3 -----
    // XML Input:
    // clan_a: True, MAR=100, RT=60, PTR=2
    // clan_b: False
    // Road: clan_a to clan_b, Time=10
    // Queries:
    // 0: Process inputs
    // 1: Attack on clan_b with 50 RR & 20 GCO
    // 125: Gold captured query -> Expected: "Gold captured: 20"
    // 130: Victory of Codeopia
    {
        Simulation sim;
        sim.addClan(Clan("clan_a", true, 100, 2, 60));
        sim.addClan(Clan("clan_b", false, 0, 0, 0));
        sim.addRoad("clan_a", "clan_b", 10);

        sim.processAttack(1, "clan_b", 50, 20.0);
        sim.processGoldQuery(125); // Expected: "Gold captured: 20"
        sim.processEvents(130);
    }

    return 0;
}
