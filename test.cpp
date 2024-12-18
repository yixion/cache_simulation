#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <deque>
#include <cmath>
#include <iomanip>
#include <cstring>
#include <algorithm>

using namespace std;

struct CacheLine {
    unsigned long int tag;
    bool dirty;
    CacheLine() : tag(0), dirty(true) {}
};

class CacheSimulator {
private:
    size_t cacheSize, blockSize, associativity;
    string replacementPolicy, inputFile;
    size_t numSets, blockOffsetBits, indexBits, tagBits;
    vector<vector<CacheLine>> cache;
    vector<deque<size_t>> lruQueues;

    // Statistics
    size_t demandFetch = 0;
    size_t cacheHit = 0;
    size_t cacheMiss = 0;
    size_t readData = 0;
    size_t writeData = 0;
    size_t bytesFromMemory = 0;
    size_t bytesToMemory = 0;
    size_t write_num_in_cache = 0;

public:
    CacheSimulator(size_t cs, size_t bs, size_t assoc, const string &policy, const string &file)
        : cacheSize(cs * 1024), blockSize(bs), associativity(assoc), replacementPolicy(policy), inputFile(file) {
        numSets = cacheSize / (blockSize * associativity);
        blockOffsetBits = log2(blockSize);
        indexBits = log2(numSets);
        tagBits = 32 - indexBits - blockOffsetBits;
        cache.resize(numSets, vector<CacheLine>(associativity));
        lruQueues.resize(numSets);
    }

    void simulate() {
        ifstream file(inputFile);
        if (!file.is_open()) {
            cerr << "Error: Unable to open file " << inputFile << endl;
            return;
        }

        string label;
        unsigned int address;
        while (file >> label >> hex >> address) {
            ++demandFetch;

            size_t index = (address >> blockOffsetBits) & ((1 << indexBits) - 1);
            unsigned long int tag = address >> (indexBits + blockOffsetBits);
            bool isWrite = (label == "1");

            if (label == "0") ++readData;
            else if (label == "1") ++writeData;

            bool hit = false;
            for (size_t i = 0; i < associativity; ++i) {
                if (cache[index][i].tag == tag) {
                    hit = true;
                    ++cacheHit;

                    // Update LRU and dirty bit
                    if (replacementPolicy == "LRU") {
                        updateLRU(index, i);
                    }

                    // Set dirty bit for writes
                    if (isWrite) {
                        if(!cache[index][i].dirty){
                            write_num_in_cache++;
                        }
                        cache[index][i].dirty = true;
                    }
                    break;
                }
            }

            if (!hit) {
                ++cacheMiss;
                bytesFromMemory += blockSize; // Fetch block from memory on a miss
                handleMiss(index, tag, isWrite);
            }
        }
        file.close();
        outputResults();
    }

private:
    void updateLRU(size_t setIndex, size_t lineIndex) {
        auto &queue = lruQueues[setIndex];
        auto it = std::find(queue.begin(), queue.end(), lineIndex);
        if (it != queue.end()) {
            queue.erase(it);
        }
        queue.push_back(lineIndex);
    }


    void handleMiss(size_t setIndex, unsigned int tag, bool isWrite) {
        auto &queue = lruQueues[setIndex];
        size_t victimLine = 0;
        // Eviction process
        if (queue.size() < associativity) {
            victimLine = queue.size(); // New line added to cache
        } else {
            // Find victim line based on replacement policy
            victimLine = (replacementPolicy == "LRU") ? queue.front() : queue.front();
            queue.pop_front(); // Remove the evicted line from the queue
            if(cache[setIndex][victimLine].dirty){
                bytesToMemory += blockSize;
                write_num_in_cache--;
            }
        }
        // Update the cache with the new block
        queue.push_back(victimLine);
        cache[setIndex][victimLine].tag = tag;
        if(isWrite){
            cache[setIndex][victimLine].dirty = true;
            write_num_in_cache++;
        }else{
            cache[setIndex][victimLine].dirty = false;
        }
        
    }


    void outputResults() const {
        cout << "Input file: " << inputFile << endl;
        cout << "Demand fetch: " << demandFetch << endl;
        cout << "Cache hit: " << cacheHit << endl;
        cout << "Cache miss: " << cacheMiss << endl;
        cout << fixed << setprecision(4) << "Miss rate: " << (cacheMiss * 1.0 / demandFetch) << endl;
        cout << "Read data: " << readData << endl;
        cout << "Write data: " << writeData << endl;
        cout << "Bytes from memory: " << bytesFromMemory << endl;
        cout << "Bytes to memory: " << bytesToMemory + blockSize * write_num_in_cache << endl;
    }
};

int main(int argc, char *argv[]) {
    if (argc != 6) {
        cerr << "Usage: ./cache [cache size] [block size] [associativity] [replace policy] [file name]" << endl;
        return 1;
    }

    size_t cacheSize = stoi(argv[1]);
    size_t blockSize = stoi(argv[2]);
    size_t associativity = (argv[3][0] == 'f') ? cacheSize * 1024 / blockSize : stoi(argv[3]);
    string replacementPolicy = argv[4];
    string inputFile = argv[5];

    CacheSimulator simulator(cacheSize, blockSize, associativity, replacementPolicy, inputFile);
    simulator.simulate();

    return 0;
}
