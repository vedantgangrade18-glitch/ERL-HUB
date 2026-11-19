#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;

namespace ReliefOps {

inline string num(double v) {
    ostringstream os;
    os << fixed << setprecision(1) << v;
    return os.str();
}

class ReliefException : public exception {
protected:
    string message;
public:
    explicit ReliefException(const string& m) : message(m) {}
    virtual ~ReliefException() noexcept {}
    const char* what() const noexcept override { return message.c_str(); }
};

class CargoOverweightException : public ReliefException {
public:
    CargoOverweightException(const string& vehicleId, double load, double limit)
        : ReliefException("CargoOverweightException: " + vehicleId + " cannot lift " +
                          num(load) + " kg (limit " + num(limit) + " kg)") {}
};

class FuelDepletionException : public ReliefException {
public:
    FuelDepletionException(const string& vehicleId, double needed, double available)
        : ReliefException("FuelDepletionException: " + vehicleId + " needs " + num(needed) +
                          " L but has only " + num(available) + " L") {}
};

class DamagedCorridorException : public ReliefException {
public:
    DamagedCorridorException(const string& vehicleId, const string& vehicleType)
        : ReliefException("DamagedCorridorException: corridor is damaged, " + vehicleType +
                          " " + vehicleId + " cannot pass") {}
};

inline ostream& tableRule(ostream& os) { return os << string(46, '-') << '\n'; }

inline ostream& tableHeader(ostream& os) {
    ostringstream h;
    h << left << setw(18) << "Item" << setw(8) << "Qty" << setw(12) << "Weight(kg)" << "Priority";
    return os << h.str() << '\n' << tableRule;
}

namespace Logistics { class IncidentCommander; }

namespace Inventory {

class ReliefCargo {
    string name;
    int quantity;
    double unitWeightKg;
    int priority;
    friend class Logistics::IncidentCommander;
public:
    ReliefCargo(const string& n = "", int q = 0, double w = 0.0, int p = 3)
        : name(n), quantity(q), unitWeightKg(w), priority(p) {}

    const string& getName() const { return name; }
    int getQuantity() const { return quantity; }
    double getUnitWeightKg() const { return unitWeightKg; }
    int getPriority() const { return priority; }
    inline double totalWeightKg() const { return quantity * unitWeightKg; }

    bool operator==(const ReliefCargo& o) const {
        return name == o.name && quantity == o.quantity && unitWeightKg == o.unitWeightKg;
    }

    friend ostream& operator<<(ostream& os, const ReliefCargo& c);
    friend istream& operator>>(istream& is, ReliefCargo& c);
};

ostream& operator<<(ostream& os, const ReliefCargo& c) {
    ostringstream row;
    row << left << setw(18) << c.name << setw(8) << c.quantity
        << setw(12) << num(c.totalWeightKg()) << c.priority;
    return os << row.str();
}

istream& operator>>(istream& is, ReliefCargo& c) {
    is >> c.name >> c.quantity >> c.unitWeightKg >> c.priority;
    replace(c.name.begin(), c.name.end(), '_', ' ');
    return is;
}

class Manifest {
    string id;
    int capacity;
    ReliefCargo* items;
    int count;

    void grow() {
        int newCap = capacity * 2;
        ReliefCargo* bigger = new ReliefCargo[newCap];
        for (int i = 0; i < count; ++i) bigger[i] = items[i];
        delete[] items;
        items = bigger;
        capacity = newCap;
    }
public:
    explicit Manifest(const string& manifestId = "MF-000", int cap = 4)
        : id(manifestId), capacity(cap > 0 ? cap : 1), items(new ReliefCargo[capacity]), count(0) {}

    Manifest(const Manifest& o)
        : id(o.id), capacity(o.capacity), items(new ReliefCargo[o.capacity]), count(o.count) {
        for (int i = 0; i < count; ++i) items[i] = o.items[i];
    }

    Manifest& operator=(const Manifest& o) {
        if (this != &o) {
            ReliefCargo* copy = new ReliefCargo[o.capacity];
            for (int i = 0; i < o.count; ++i) copy[i] = o.items[i];
            delete[] items;
            items = copy;
            id = o.id;
            capacity = o.capacity;
            count = o.count;
        }
        return *this;
    }

    ~Manifest() { delete[] items; }

    void addItem(const ReliefCargo& c) {
        if (count == capacity) grow();
        items[count++] = c;
    }
    const string& getId() const { return id; }
    int size() const { return count; }

    double totalWeightKg() const {
        double sum = 0.0;
        for (int i = 0; i < count; ++i) sum += items[i].totalWeightKg();
        return sum;
    }

    bool hasDuplicate(const ReliefCargo& c) const {
        for (int i = 0; i < count; ++i)
            if (items[i] == c) return true;
        return false;
    }

    bool operator==(const Manifest& o) const {
        if (count != o.count) return false;
        for (int i = 0; i < count; ++i)
            if (!(items[i] == o.items[i])) return false;
        return true;
    }

    friend ostream& operator<<(ostream& os, const Manifest& m);
};

ostream& operator<<(ostream& os, const Manifest& m) {
    os << "Manifest " << m.id << " (" << m.count << " lines)\n" << tableHeader;
    for (int i = 0; i < m.count; ++i) os << m.items[i] << '\n';
    return os << tableRule << "Total weight: " << num(m.totalWeightKg()) << " kg\n";
}

istream& operator>>(istream& is, Manifest& m) {
    string id;
    int n = 0;
    if (!(is >> id >> n)) return is;
    Manifest fresh(id, n);
    for (int i = 0; i < n; ++i) {
        ReliefCargo c;
        if (!(is >> c)) return is;
        fresh.addItem(c);
    }
    m = fresh;
    return is;
}

class DistributionDepot {
    string name;
    unordered_map<string, int> stock;
    friend class Logistics::IncidentCommander;
    friend ostream& operator<<(ostream& os, const DistributionDepot& d);
public:
    explicit DistributionDepot(const string& n) : name(n) {}
    const string& getName() const { return name; }

    int& stockOf(const string& item) { return stock[item]; }
    int stockOf(const string& item) const {
        auto it = stock.find(item);
        return it == stock.end() ? 0 : it->second;
    }

    DistributionDepot& operator+=(const ReliefCargo& c) {
        stock[c.getName()] += c.getQuantity();
        return *this;
    }
    DistributionDepot& operator-=(const ReliefCargo& c) {
        auto it = stock.find(c.getName());
        if (it == stock.end() || it->second < c.getQuantity())
            throw runtime_error("Insufficient stock of " + c.getName() + " at " + name);
        it->second -= c.getQuantity();
        return *this;
    }
};

ostream& operator<<(ostream& os, const DistributionDepot& d) {
    vector< pair<string, int> > rows(d.stock.begin(), d.stock.end());
    sort(rows.begin(), rows.end());
    ostringstream out;
    out << "Depot: " << d.name << '\n';
    for (size_t i = 0; i < rows.size(); ++i)
        out << "  " << left << setw(18) << rows[i].first << right << setw(6) << rows[i].second << '\n';
    return os << out.str();
}

}

namespace Logistics {

inline double distanceKm(double x1, double y1, double x2, double y2) {
    return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}
inline double toTonnes(double kg) { return kg / 1000.0; }

int allocateSupply(const Inventory::ReliefCargo& c, double payloadLimitKg) {
    if (c.getUnitWeightKg() <= 0.0) return 0;
    int fit = static_cast<int>(payloadLimitKg / c.getUnitWeightKg());
    return min(fit, c.getQuantity());
}
int allocateSupply(const Inventory::ReliefCargo& c, int availableUnits, bool medicalTriage) {
    if (medicalTriage) return min(c.getQuantity(), availableUnits);
    return min(c.getQuantity(), availableUnits / 2);
}

void updateStock(int& stock, int delta) { stock += delta; }
double& applyDetour(double& distance, double factor) {
    distance *= factor;
    return distance;
}

class IncidentCommander {
public:
    void emergencyStockOverride(Inventory::DistributionDepot& d, const string& item, int qty) {
        d.stock[item] += qty;
    }
    void escalateToCritical(Inventory::ReliefCargo& c) {
        c.priority = 1;
    }
};

class GPSBeacon {
    double x, y;
public:
    GPSBeacon(double px = 0.0, double py = 0.0) : x(px), y(py) {}
    void moveBy(double dx, double dy) { x += dx; y += dy; }
    double getX() const { return x; }
    double getY() const { return y; }
};

class EmergencyLifeSupportUnit {
    int oxygenPercent;
    bool online;
public:
    EmergencyLifeSupportUnit(int o = 100, bool on = true) : oxygenPercent(o), online(on) {}
    bool isReady() const { return online && oxygenPercent >= 20; }
};

class ReliefVehicle {
    string id;
    double capacityKg;
    double fuelLitres;
    double fuelPerKm;
    GPSBeacon gps;
    EmergencyLifeSupportUnit lifeSupport;

    static int operationalVehicles;
    static double totalDispatchedTonnes;
public:
    ReliefVehicle(const string& vid, double cap, double fuel, double burn)
        : id(vid), capacityKg(cap), fuelLitres(fuel), fuelPerKm(burn) {
        ++operationalVehicles;
    }
    ReliefVehicle(const ReliefVehicle&) = delete;
    ReliefVehicle& operator=(const ReliefVehicle&) = delete;

    virtual ~ReliefVehicle() {
        --operationalVehicles;
        cout << "  [~ReliefVehicle] " << id << " released\n";
    }

    virtual double evaluateRoute(double distanceKm, bool corridorDamaged) const = 0;
    virtual string type() const = 0;

    const string& getId() const { return id; }

    double dispatch(const Inventory::Manifest& m, double distanceKm, bool corridorDamaged) {
        double load = m.totalWeightKg();
        if (load > capacityKg) throw CargoOverweightException(id, load, capacityKg);
        double eta = evaluateRoute(distanceKm, corridorDamaged);
        double fuelNeeded = distanceKm * fuelPerKm;
        if (fuelNeeded > fuelLitres) throw FuelDepletionException(id, fuelNeeded, fuelLitres);
        fuelLitres -= fuelNeeded;
        totalDispatchedTonnes += toTonnes(load);
        gps.moveBy(distanceKm, 0.0);
        return eta;
    }

    void printStatus() const {
        cout << left << setw(8) << id << setw(24) << type() << "fuel " << setw(8) << num(fuelLitres)
             << "GPS(" << num(gps.getX()) << "," << num(gps.getY()) << ")  life-support "
             << (lifeSupport.isReady() ? "READY" : "OFFLINE") << '\n';
    }

    static int getOperationalVehicles() { return operationalVehicles; }
    static double getTotalDispatchedTonnes() { return totalDispatchedTonnes; }
};
int ReliefVehicle::operationalVehicles = 0;
double ReliefVehicle::totalDispatchedTonnes = 0.0;

typedef ReliefVehicle TransportVehicle;

class AerialTransport : public ReliefVehicle {
public:
    AerialTransport(const string& id, double cap, double fuel) : ReliefVehicle(id, cap, fuel, 1.5) {}
    ~AerialTransport() override { cout << "  [~AerialTransport] " << getId() << '\n'; }
    double evaluateRoute(double km, bool) const override { return km / 200.0; }
    string type() const override { return "Aerial Transport"; }
};

class GroundConvoy : public virtual ReliefVehicle {
public:
    GroundConvoy(const string& id, double cap, double fuel) : ReliefVehicle(id, cap, fuel, 0.4) {}
    ~GroundConvoy() override { cout << "  [~GroundConvoy] " << getId() << '\n'; }
    double evaluateRoute(double km, bool damaged) const override {
        if (damaged) throw DamagedCorridorException(getId(), type());
        return km / 40.0;
    }
    string type() const override { return "Ground Convoy"; }
};

class AmphibiousCraft : public virtual ReliefVehicle {
public:
    AmphibiousCraft(const string& id, double cap, double fuel) : ReliefVehicle(id, cap, fuel, 0.8) {}
    ~AmphibiousCraft() override { cout << "  [~AmphibiousCraft] " << getId() << '\n'; }
    double evaluateRoute(double km, bool) const override { return km / 25.0; }
    string type() const override { return "Amphibious Craft"; }
};

class AmphibiousMedicalUnit : public GroundConvoy, public AmphibiousCraft {
public:
    explicit AmphibiousMedicalUnit(const string& id)
        : ReliefVehicle(id, 3000.0, 400.0, 0.5),
          GroundConvoy(id, 3000.0, 400.0),
          AmphibiousCraft(id, 3000.0, 400.0) {}
    ~AmphibiousMedicalUnit() override { cout << "  [~AmphibiousMedicalUnit] " << getId() << '\n'; }
    double evaluateRoute(double km, bool damaged) const override {
        return damaged ? km / 25.0 : km / 40.0;
    }
    string type() const override { return "Amphibious Medical Unit"; }
};

vector<ReliefVehicle*> buildFleet() {
    vector<ReliefVehicle*> fleet;
    fleet.push_back(new AerialTransport("HELI-01", 1500.0, 900.0));
    fleet.push_back(new GroundConvoy("TRK-01", 8000.0, 500.0));
    fleet.push_back(new AmphibiousCraft("BOAT-01", 2500.0, 300.0));
    fleet.push_back(new AmphibiousMedicalUnit("AMU-01"));
    return fleet;
}

struct UrgencyFirst {
    bool operator()(const Inventory::ReliefCargo& a, const Inventory::ReliefCargo& b) const {
        return a.getPriority() < b.getPriority();
    }
};

template <typename T, typename Cmp>
class PriorityDispatchQueue {
    vector<T> data;
    Cmp cmp;
public:
    void push(const T& item) {
        data.push_back(item);
        stable_sort(data.begin(), data.end(), cmp);
    }
    T pop() {
        if (data.empty()) throw out_of_range("PriorityDispatchQueue is empty");
        T top = data.front();
        data.erase(data.begin());
        return top;
    }
    bool empty() const { return data.empty(); }
    size_t size() const { return data.size(); }
};

struct TransitRecord {
    char vehicleId[16];
    char vehicleType[24];
    double distanceKm;
    double cargoKg;
    double etaHours;
};

TransitRecord makeRecord(const string& id, const string& type, double km, double kg, double eta) {
    TransitRecord r;
    memset(&r, 0, sizeof(r));
    strncpy(r.vehicleId, id.c_str(), sizeof(r.vehicleId) - 1);
    strncpy(r.vehicleType, type.c_str(), sizeof(r.vehicleType) - 1);
    r.distanceKm = km;
    r.cargoKg = kg;
    r.etaHours = eta;
    return r;
}

void saveTransitLogs(const vector<TransitRecord>& logs, const string& path) {
    string tmp = path + ".tmp";
    {
        fstream out(tmp.c_str(), ios::out | ios::binary | ios::trunc);
        if (!out) throw runtime_error("Cannot open " + tmp);
        for (size_t i = 0; i < logs.size(); ++i)
            out.write(reinterpret_cast<const char*>(&logs[i]), sizeof(TransitRecord));
        out.flush();
        if (!out) throw runtime_error("Write failed, old log kept");
    }
    std::remove(path.c_str());
    if (std::rename(tmp.c_str(), path.c_str()) != 0)
        throw runtime_error("Could not replace " + path);
}

vector<TransitRecord> loadTransitLogs(const string& path) {
    vector<TransitRecord> logs;
    fstream in(path.c_str(), ios::in | ios::binary);
    if (!in) return logs;
    TransitRecord r = TransitRecord();
    while (in.read(reinterpret_cast<char*>(&r), sizeof(TransitRecord))) logs.push_back(r);
    return logs;
}

inline ostream& logHeader(ostream& os) {
    ostringstream h;
    h << left << setw(10) << "Vehicle" << setw(26) << "Type" << setw(12) << "Dist(km)"
      << setw(12) << "Cargo(kg)" << "ETA(h)";
    return os << h.str() << '\n' << string(66, '-') << '\n';
}

void printRecord(const TransitRecord& r) {
    ostringstream row;
    row << left << setw(10) << r.vehicleId << setw(26) << r.vehicleType << setw(12) << num(r.distanceKm)
        << setw(12) << num(r.cargoKg) << num(r.etaHours);
    cout << row.str() << '\n';
}

void tryDispatch(ReliefVehicle* v, const Inventory::Manifest& m, double km, bool damaged,
                 vector<TransitRecord>& logs) {
    cout << "Dispatch " << v->getId() << ": " << num(km) << " km, load " << num(m.totalWeightKg())
         << " kg" << (damaged ? ", corridor DAMAGED" : "") << '\n';
    try {
        double eta = v->dispatch(m, km, damaged);
        cout << "   OK, ETA " << num(eta) << " h\n";
        logs.push_back(makeRecord(v->getId(), v->type(), km, m.totalWeightKg(), eta));
    } catch (const exception& e) {
        cout << "   FAILED -> " << e.what() << '\n';
    }
}

}
}

using namespace ReliefOps;

void banner(const string& title) { cout << "\n======== " << title << " ========\n"; }

void demoUnit1(Inventory::DistributionDepot& depot) {
    banner("UNIT I : namespaces, overloading, references, inline, friend");
    using Inventory::ReliefCargo;

    depot.stockOf("Ration Kit") = 500;
    depot.stockOf("Trauma Medicine") = 100;

    ReliefCargo rations("Ration Kit", 500, 2.5, 3);
    cout << "Overload 1 (payload limit 600 kg)        : " << Logistics::allocateSupply(rations, 600.0) << " kits\n";
    cout << "Overload 2 (medical triage, 400 in stock): " << Logistics::allocateSupply(rations, 400, true) << " kits\n";
    cout << "Overload 2 (normal request, 400 in stock): " << Logistics::allocateSupply(rations, 400, false) << " kits\n";

    int& rationStock = depot.stockOf("Ration Kit");
    Logistics::updateStock(rationStock, -100);
    cout << "Ration Kit stock after updateStock(-100)  : " << depot.stockOf("Ration Kit") << '\n';

    double route = Logistics::distanceKm(0, 0, 30, 40);
    cout << "Inline distanceKm(0,0,30,40)              : " << num(route) << " km\n";
    Logistics::applyDetour(route, 1.2) += 5.0;
    cout << "After detour x1.2 then +5 via reference  : " << num(route) << " km\n";

    Logistics::IncidentCommander commander;
    commander.emergencyStockOverride(depot, "Trauma Medicine", 150);
    cout << "Friend override, Trauma Medicine stock   : " << depot.stockOf("Trauma Medicine") << '\n';
    ReliefCargo med("Trauma Medicine", 100, 1.2, 3);
    cout << "Priority before escalation: " << med.getPriority();
    commander.escalateToCritical(med);
    cout << ", after: " << med.getPriority() << '\n';
}

void demoUnit2(Inventory::DistributionDepot& depot) {
    banner("UNIT II : ADT, static, deep copy, operators");
    using Inventory::ReliefCargo;
    using Inventory::Manifest;

    ReliefCargo water("Water Tank", 5, 1000.0, 2);
    ReliefCargo blankets("Thermal Blanket", 300, 0.8, 3);
    depot += water;
    depot += blankets;
    cout << depot;

    cout << "\nVehicle departs: deduct 2 water tanks (operator -=)\n";
    depot -= ReliefCargo("Water Tank", 2, 1000.0, 2);
    cout << depot;
    try {
        depot -= ReliefCargo("Water Tank", 50, 1000.0, 2);
    } catch (const exception& e) {
        cout << "Rejected: " << e.what() << '\n';
    }

    cout << "\n-- Dynamic manifest, deep copy, operator == --\n";
    Manifest m1("MF-101", 2);
    m1.addItem(ReliefCargo("Ration Kit", 100, 2.5, 3));
    m1.addItem(ReliefCargo("Trauma Medicine", 40, 1.2, 1));
    m1.addItem(blankets);
    Manifest m2(m1);
    cout << "m1 == m2 right after copying ? " << (m1 == m2 ? "yes" : "no") << '\n';
    m2.addItem(water);
    cout << "After adding a line to the copy: m1 has " << m1.size() << " lines, m2 has " << m2.size()
         << ", equal? " << (m1 == m2 ? "yes" : "no") << '\n';
    cout << "Duplicate check, m1 already has these blankets? " << (m1.hasDuplicate(blankets) ? "yes" : "no") << "\n\n";
    cout << m1;

    cout << "\n-- operator >> reading a manifest from text --\n";
    istringstream in("MF-202 2 Ration_Kit 100 2.5 2 Trauma_Medicine 50 1.2 1");
    Manifest m3;
    in >> m3;
    cout << m3;
}

void demoUnit3(vector<Logistics::ReliefVehicle*>& fleet) {
    banner("UNIT III : virtual base, containership, polymorphism");
    cout << "Operational vehicles (static member): " << Logistics::ReliefVehicle::getOperationalVehicles() << "\n\n";
    for (size_t i = 0; i < fleet.size(); ++i) fleet[i]->printStatus();

    cout << "\nRoute evaluation through ReliefVehicle* for 60 km (runtime polymorphism):\n";
    for (size_t i = 0; i < fleet.size(); ++i) {
        Logistics::ReliefVehicle* v = fleet[i];
        for (int damaged = 0; damaged < 2; ++damaged) {
            string result;
            try {
                result = num(v->evaluateRoute(60.0, damaged != 0)) + " h";
            } catch (const exception& e) {
                result = e.what();
            }
            cout << "  " << left << setw(8) << v->getId() << setw(24) << v->type()
                 << (damaged ? "damaged: " : "clear:   ") << result << '\n';
        }
    }
}

void demoUnit4(vector<Logistics::ReliefVehicle*>& fleet) {
    banner("UNIT IV : exceptions, binary files, STL, template, manipulators");
    using Inventory::ReliefCargo;
    using Inventory::Manifest;

    Manifest light("MF-LIGHT", 3);
    light.addItem(ReliefCargo("Trauma Medicine", 100, 1.2, 1));
    light.addItem(ReliefCargo("Ration Kit", 200, 2.5, 3));
    Manifest heavy("MF-HEAVY", 2);
    heavy.addItem(ReliefCargo("Water Tank", 4, 1000.0, 2));

    vector<Logistics::TransitRecord> logs;
    Logistics::ReliefVehicle* heli = fleet[0];
    Logistics::ReliefVehicle* truck = fleet[1];
    Logistics::ReliefVehicle* boat = fleet[2];
    Logistics::ReliefVehicle* amu = fleet[3];

    Logistics::tryDispatch(heli, light, 80.0, false, logs);
    Logistics::tryDispatch(heli, heavy, 80.0, false, logs);
    Logistics::tryDispatch(truck, light, 120.0, true, logs);
    Logistics::tryDispatch(truck, heavy, 120.0, false, logs);
    Logistics::tryDispatch(amu, light, 1000.0, false, logs);
    Logistics::tryDispatch(amu, light, 150.0, true, logs);
    Logistics::tryDispatch(boat, light, 60.0, true, logs);
    cout << "\nTotal dispatched so far (static member): "
         << num(Logistics::ReliefVehicle::getTotalDispatchedTonnes()) << " tonnes\n";

    cout << "\n-- Binary log: save, reload, atomic update --\n";
    const string file = "transit_log.bin";
    Logistics::saveTransitLogs(logs, file);
    vector<Logistics::TransitRecord> loaded = Logistics::loadTransitLogs(file);
    cout << "Saved " << logs.size() << " records, reloaded " << loaded.size() << " records\n";
    cout << Logistics::logHeader;
    for_each(loaded.begin(), loaded.end(), Logistics::printRecord);

    loaded.push_back(Logistics::makeRecord("HELI-01", "Aerial Transport", 40.0, 300.0, 0.2));
    Logistics::saveTransitLogs(loaded, file);
    cout << "After atomic update the file holds " << Logistics::loadTransitLogs(file).size() << " records\n";

    cout << "\n-- Functor sorting + generic PriorityDispatchQueue<T> --\n";
    vector<ReliefCargo> needs;
    needs.push_back(ReliefCargo("Thermal Blanket", 300, 0.8, 3));
    needs.push_back(ReliefCargo("Clean Water", 10, 1000.0, 2));
    needs.push_back(ReliefCargo("Trauma Medicine", 100, 1.2, 1));
    needs.push_back(ReliefCargo("Ration Kit", 200, 2.5, 3));

    Logistics::PriorityDispatchQueue<ReliefCargo, Logistics::UrgencyFirst> dispatchQ;
    for (size_t i = 0; i < needs.size(); ++i) dispatchQ.push(needs[i]);
    cout << "Dispatch order from the queue:\n" << tableHeader;
    while (!dispatchQ.empty()) cout << dispatchQ.pop() << '\n';
    cout << tableRule;

    sort(needs.begin(), needs.end(), Logistics::UrgencyFirst());
    cout << "\nstd::sort with the same functor, printed with std::for_each:\n";
    for_each(needs.begin(), needs.end(), [](const ReliefCargo& c) { cout << c << '\n'; });
}

int main() {
    Inventory::DistributionDepot depot("Uttarkashi Field Hub");
    vector<Logistics::ReliefVehicle*> fleet;

    try {
        demoUnit1(depot);
        demoUnit2(depot);
        fleet = Logistics::buildFleet();
        demoUnit3(fleet);
        demoUnit4(fleet);
    } catch (const exception& e) {
        cout << "Fatal error: " << e.what() << '\n';
    }

    banner("CLEANUP : virtual destructors via base pointers");
    for (size_t i = 0; i < fleet.size(); ++i) delete fleet[i];
    fleet.clear();
    cout << "Operational vehicles now: " << Logistics::ReliefVehicle::getOperationalVehicles() << '\n';
    return 0;
}
