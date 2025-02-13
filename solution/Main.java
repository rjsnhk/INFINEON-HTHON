import java.io.File;
import java.util.*;
import javax.xml.parsers.*;
import org.w3c.dom.*;

class Clan {
    String name;
    boolean isMine;
    int MAR;                // maximum available resource
    int PTR;                // processing time per resource
    int RT;                 // refill time
    int currentAvailable;   // current available resource
    boolean busy;           // true when mine extraction is busy
    double extractionStartTime; // extraction start time
    int extractionRequest;      // resource extraction request

    // For blocking
    boolean blocked;             // true if clan is blocked
    double blockedUntil;         // block duration
    boolean firstRequestDuringBlock; // flag for first request during block

    // while extraction
    double effectivePTR;      // actual processing time per resource (adjusted during block)
    double ongoingGCO;        // current attack's gold capturing opportunity

    Clan(String _name, boolean _isMine, int _MAR, int _PTR, int _RT) {
        name = _name;
        isMine = _isMine;
        MAR = _MAR;
        PTR = _PTR;
        RT = _RT;
        busy = false;
        currentAvailable = _MAR;
        extractionStartTime = 0;
        extractionRequest = 0;
        // Initially, no block
        blocked = false;
        blockedUntil = 0;
        firstRequestDuringBlock = false;
        effectivePTR = PTR; // initially normal speed
        ongoingGCO = 0;
    }
}

enum EventType {
    TROOP_ARRIVAL,       // troops arrive at the supplier mine and begin extraction
    EXTRACTION_COMPLETE, // mining completes at the supplier mine
    TROOP_RETURN,        // troops return with resources
    MINE_REFILL,         // mine is refilled after RT seconds
    BLOCK,               // block a clan
    UNBLOCK              // unblock a clan
}

class AttackRequest {
    String targetClan;   // target clan for attack
    int required;        // resources required to defend (RR)
    double gco;          // gold capturing opportunity (if attack is successful)
    String supplierMine; // supplier mine for resource supply

    AttackRequest(String _targetClan, int _required, double _gco, String _supplierMine) {
        targetClan = _targetClan;
        required = _required;
        gco = _gco;
        supplierMine = _supplierMine;
    }
}

class Event implements Comparable<Event> {
    double time;
    EventType type;
    AttackRequest attack;  // used for troop arrival, extraction, and return events
    String mineName;       // used for mine refill events, block, unblock
    int blockDuration;     // block duration

    Event(double _time, EventType _type, AttackRequest _attack, String _mineName, int _blockDuration) {
        time = _time;
        type = _type;
        attack = _attack;
        mineName = _mineName;
        blockDuration = _blockDuration;
    }

    @Override
    public int compareTo(Event other) {
        return Double.compare(this.time, other.time);
    }
}

class Simulation {
    private double currentTime;
    private double goldCaptured;
    private Map<String, Clan> clans;  // map clan name to clan data
    private Map<String, List<Pair<String, Integer>>> graph; // for each clan, list of (neighbor, travel time)
    private PriorityQueue<Event> eventQueue;
    private final double slowdownFactor = 0.7;

    Simulation() {
        currentTime = 0;
        goldCaptured = 0;
        clans = new HashMap<>();
        graph = new HashMap<>();
        eventQueue = new PriorityQueue<>();
    }

    // add clan to the simulation
    void addClan(Clan clan) {
        clans.put(clan.name, clan);
    }

    // add road (bidirectional)
    void addRoad(String from, String to, int travelTime) {
        graph.computeIfAbsent(from, k -> new ArrayList<>()).add(new Pair<>(to, travelTime));
        graph.computeIfAbsent(to, k -> new ArrayList<>()).add(new Pair<>(from, travelTime));
    }

    // Dijkstra's to calculate travel time between clans
    int shortestTravelTime(String start, String end) {
        Map<String, Integer> dist = new HashMap<>();
        for (String clanName : clans.keySet()) {
            dist.put(clanName, Integer.MAX_VALUE); // infinity
        }
        PriorityQueue<Pair<Integer, String>> pq = new PriorityQueue<>(Comparator.comparingInt(Pair::getKey));
        dist.put(start, 0);
        pq.add(new Pair<>(0, start));
        while (!pq.isEmpty()) {
            Pair<Integer, String> current = pq.poll();
            int d = current.getKey();
            String u = current.getValue();
            if (d > dist.get(u)) continue;
            if (u.equals(end)) return d;
            for (Pair<String, Integer> edge : graph.getOrDefault(u, new ArrayList<>())) {
                String v = edge.getKey();
                int w = edge.getValue();
                if (dist.get(u) + w < dist.get(v)) {
                    dist.put(v, dist.get(u) + w);
                    pq.add(new Pair<>(dist.get(v), v));
                }
            }
        }
        return Integer.MAX_VALUE;
    }

    // schedule a new event
    void scheduleEvent(Event e) {
        eventQueue.add(e);
    }

    // Process all events scheduled until next query time
    void processEvents(double nextQueryTime) {
        while (!eventQueue.isEmpty() && eventQueue.peek().time <= nextQueryTime) {
            Event e = eventQueue.poll();
            currentTime = e.time;
            switch (e.type) {
                case TROOP_ARRIVAL: {
                    Clan supplier = clans.get(e.attack.supplierMine);
                    supplier.busy = true;
                    supplier.extractionStartTime = currentTime;
                    supplier.extractionRequest = e.attack.required;
                    supplier.ongoingGCO = e.attack.gco;
                    double extractionCompleteTime;
                    if (supplier.blocked && currentTime < supplier.blockedUntil) {
                        if (!supplier.firstRequestDuringBlock) {
                            supplier.firstRequestDuringBlock = true;
                            supplier.effectivePTR = supplier.PTR; // first request at normal speed
                        } else {
                            supplier.effectivePTR = supplier.PTR * slowdownFactor; // subsequent requests at reduced speed
                        }
                        extractionCompleteTime = currentTime + supplier.extractionRequest * supplier.effectivePTR;
                    } else {
                        supplier.effectivePTR = supplier.PTR;
                        extractionCompleteTime = currentTime + supplier.extractionRequest * supplier.effectivePTR;
                    }
                    Event extractionComplete = new Event(extractionCompleteTime, EventType.EXTRACTION_COMPLETE, e.attack, null, 0);
                    scheduleEvent(extractionComplete);
                    break;
                }
                case EXTRACTION_COMPLETE: {
                    Clan supplier = clans.get(e.attack.supplierMine);
                    supplier.currentAvailable -= e.attack.required;
                    supplier.busy = false;
                    int travelTime = shortestTravelTime(e.attack.targetClan, supplier.name);
                    double troopReturnTime = currentTime + travelTime;
                    Event troopReturn = new Event(troopReturnTime, EventType.TROOP_RETURN, e.attack, null, 0);
                    scheduleEvent(troopReturn);
                    Event refill = new Event(currentTime + supplier.RT, EventType.MINE_REFILL, null, supplier.name, 0);
                    scheduleEvent(refill);
                    break;
                }
                case TROOP_RETURN: {
                    goldCaptured += e.attack.gco;
                    break;
                }
                case MINE_REFILL: {
                    Clan mine = clans.get(e.mineName);
                    mine.currentAvailable = mine.MAR;
                    mine.busy = false;
                    break;
                }
                case BLOCK: {
                    Clan c = clans.get(e.mineName);
                    c.blocked = true;
                    c.blockedUntil = currentTime + e.blockDuration;
                    c.firstRequestDuringBlock = false; // new block period
                    break;
                }
                case UNBLOCK: {
                    Clan c = clans.get(e.mineName);
                    c.blocked = false;
                    c.firstRequestDuringBlock = false;
                    break;
                }
            }
        }
        currentTime = nextQueryTime;
    }

    // process attack at time queryTime on targetClan with given RR and GCO
    void processAttack(double queryTime, String targetClan, int RR, double GCO) {
        processEvents(queryTime);
        String bestMine = "";
        double bestTime = Double.MAX_VALUE;
        for (Map.Entry<String, Clan> entry : clans.entrySet()) {
            Clan clan = entry.getValue();
            if (clan.isMine) {
                int travelTime = shortestTravelTime(targetClan, clan.name);
                if (travelTime == Integer.MAX_VALUE) continue; // not connected
                if (clan.busy) continue; // mine is busy
                double totalTime = travelTime + RR * clan.PTR + travelTime;
                if (totalTime < bestTime) {
                    bestTime = totalTime;
                    bestMine = clan.name;
                }
            }
        }
        if (bestMine.isEmpty()) return;
        AttackRequest req = new AttackRequest(targetClan, RR, GCO, bestMine);
        int travelTime = shortestTravelTime(targetClan, bestMine);
        Event troopArrival = new Event(queryTime + travelTime, EventType.TROOP_ARRIVAL, req, null, 0);
        scheduleEvent(troopArrival);
    }

    // process block query
    void processBlockQuery(double queryTime, String clanName, int blockDuration) {
        processEvents(queryTime);
        Event blockEvent = new Event(queryTime, EventType.BLOCK, null, clanName, blockDuration);
        scheduleEvent(blockEvent);
        Event unblockEvent = new Event(queryTime + blockDuration, EventType.UNBLOCK, null, clanName, 0);
        scheduleEvent(unblockEvent);
    }

    // process status query
    void processStatusQuery(double queryTime) {
        processEvents(queryTime);
        List<Clan> mineClans = new ArrayList<>();
        for (Clan clan : clans.values()) {
            if (clan.isMine) {
                mineClans.add(clan);
            }
        }
        mineClans.sort((a, b) -> Integer.compare(b.MAR, a.MAR));
        for (Clan mine : mineClans) {
            int available;
            if (mine.busy) {
                double elapsed = currentTime - mine.extractionStartTime;
                int extracted = Math.min(mine.extractionRequest, (int) (elapsed / mine.effectivePTR));
                available = mine.MAR - extracted;
            } else {
                available = mine.currentAvailable;
            }
            System.out.print(mine.name + ": " + available + "/" + mine.MAR + " available ");
        }
        System.out.println();
    }

    // process gold query
    void processGoldQuery(double queryTime) {
        processEvents(queryTime);
        System.out.println("Gold captured: " + goldCaptured);
    }
}

class Pair<K, V> {
    private K key;
    private V value;

    Pair(K key, V value) {
        this.key = key;
        this.value = value;
    }

    public K getKey() {
        return key;
    }

    public V getValue() {
        return value;
    }
}



public class Main {
    public static void main(String[] args) {
        if (args.length < 2) {
            System.out.println("Usage: java Main <model.xml> <queries.txt>");
            return;
        }

        String xmlFile = args[0];
        String queryFile = args[1];

        Simulation sim = new Simulation();
        parseXML(xmlFile, sim);
        processQueries(queryFile, sim);
    }

    private static void parseXML(String xmlFile, Simulation sim) {
        try {
            File file = new File(xmlFile);
            DocumentBuilderFactory dbFactory = DocumentBuilderFactory.newInstance();
            DocumentBuilder dBuilder = dbFactory.newDocumentBuilder();
            Document doc = dBuilder.parse(file);
            doc.getDocumentElement().normalize();

            NodeList clanList = doc.getElementsByTagName("Clan");
            for (int i = 0; i < clanList.getLength(); i++) {
                Element clan = (Element) clanList.item(i);
                String name = clan.getElementsByTagName("Name").item(0).getTextContent();
                boolean isMine = Boolean.parseBoolean(clan.getElementsByTagName("IS_MINE").item(0).getTextContent());
                int MAR = isMine ? Integer.parseInt(clan.getElementsByTagName("MAR").item(0).getTextContent()) : 0;
                int PTR = isMine ? Integer.parseInt(clan.getElementsByTagName("PTR").item(0).getTextContent()) : 0;
                int RT = isMine ? Integer.parseInt(clan.getElementsByTagName("RT").item(0).getTextContent()) : 0;
                sim.addClan(new Clan(name, isMine, MAR, PTR, RT));
            }

            NodeList roadList = doc.getElementsByTagName("Road");
            for (int i = 0; i < roadList.getLength(); i++) {
                Element road = (Element) roadList.item(i);
                String from = road.getElementsByTagName("From").item(0).getTextContent();
                String to = road.getElementsByTagName("To").item(0).getTextContent();
                int time = Integer.parseInt(road.getElementsByTagName("Time").item(0).getTextContent());
                sim.addRoad(from, to, time);
            }
        } catch (Exception e) {
            System.out.println("Error parsing XML: " + e.getMessage());
        }
    }

    private static void processQueries(String queryFile, Simulation sim) {
        try (Scanner scanner = new Scanner(new File(queryFile))) {
            while (scanner.hasNextLine()) {
                String line = scanner.nextLine();
                String[] parts = line.split(": ", 2);
                int time = Integer.parseInt(parts[0]);
                String command = parts[1];

                if (command.startsWith("Attack on")) {
                    String[] attackParts = command.split(" ");
                    String clan = attackParts[2];
                    int resources = Integer.parseInt(attackParts[4]);
                    double gco = Double.parseDouble(attackParts[6]);
                    sim.processAttack(time, clan, resources, gco);
                } else if (command.startsWith("Show the current status")) {
                    sim.processStatusQuery(time);
                } else if (command.startsWith("Produce the current amount of Gold captured")) {
                    sim.processGoldQuery(time);
                } else if (command.startsWith("Victory of Codeopia")) {
                    sim.processEvents(time);
                } else if (command.contains("has been blocked")) {
                    String[] blockParts = command.split(" ");
                    String clan = blockParts[0];
                    int duration = Integer.parseInt(blockParts[5]);
                    sim.processBlockQuery(time, clan, duration);
                }
            }
        } catch (Exception e) {
            System.out.println("Error processing queries: " + e.getMessage());
        }
    }
}
