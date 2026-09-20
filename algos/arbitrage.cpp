#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

using Curs = std::vector<std::string>;
using Graph = std::unordered_map<std::string, std::unordered_map<std::string, double>>;
using Edges = std::vector<std::tuple<std::string, std::string, double>>;
static constexpr double INF = std::numeric_limits<double>::max();
bool dfs(const std::string& node, 
        const Graph& graph, 
        std::unordered_set<std::string>& visited, 
        const std::string& base_cur,
        double ratio)
{
    if (visited.find(node) != visited.end()) {
        if (node == base_cur) {
            return ratio < 0;
        }
        return false;
    }
    bool res = false;
    if (graph.find(node) != graph.end()) {
        visited.emplace(node);
        for (const auto& [cur, r]: graph.at(node)) {
            res = res || dfs(cur, graph, visited, base_cur, ratio+r);
        }
        visited.erase(node);
    }
    return res;
}

bool DFSSolution(const Curs& curs, const Graph& graph) {
    std::unordered_set<std::string> visited;
    for(const auto& cur: curs) {
        if (dfs(cur, graph, visited, cur, 0)) {
            return true;
        }
        visited.clear();
    }
    return false;
}

bool BellmanFordsSolution(int n, const Edges& edges) {
    std::unordered_map<std::string, double> d;
    bool updated = false;
    for (int i = 0; i < n; ++i) {
        updated = false;
        for (auto [from, to, cost]: edges) {
            if (d[from] + cost < d[to]) {
                updated = true;
                d[to] = d[from] + cost;
            }
        }
    }
    return updated;
}

bool FloidUorshillSolution(int n, const Curs& curs, Graph& graph) {
    for (int t = 0; t < n; ++t) {
        const auto& tc = curs[t];
        for (int i = 0; i < n; ++i) {
            const auto& ic = curs[i];
            for (int j = 0; j < n; ++j) {
                const auto& jc = curs[j];
                if (graph.at(ic).at(tc) < INF && graph.at(tc).at(jc) < INF) {
                    graph[ic][jc] = std::min(graph.at(ic).at(jc), graph.at(ic).at(tc) + graph.at(tc).at(jc));
                }
            }
        }
    }
    for (int t = 0; t < n; ++t) {
        const auto& tc = curs[t];
        if (graph[tc][tc] < 0) {
            return true;
        }
    }
    return false;
}

int main(int argc, char* argv[]) {
    int C = 0;
    for (;;) {
        std::cin >> C;
        if (C == 0) {
            return 0;
        }
        Graph g;
        Curs curs(C);
        curs.reserve(C);
        for (int i = 0; i < C; ++i) {
            std::cin >> curs[i];
        }
        for (int i = 0; i < C; ++i) {
            for (int j = 0; j < C; ++j) {
                g[curs[i]][curs[j]] = INF;
            }
        }
        for (int i = 0; i < C; ++i) {
            g[curs[i]][curs[i]] = 0;
        }
        int R = 0;
        std::cin >> R;
        // Edges edges;
        // edges.reserve(R);
        for (int i = 0; i < R; ++i) {
            std::string from;
            std::string to;
            std::cin >> from;
            std::cin >> to;
            int a = 0;
            int b = 0;
            char del;
            std::cin >> a;
            std::cin>> del;
            std::cin >> b;
            g[from][to] = -std::log(b*1.0/a);
            // edges.emplace_back(from, to, -std::log(b*1.0/a));
        }
        // std::cout << (DFSSolution(curs, g) ? "Arbitrage" : "Ok") << std::endl;
        // std::cout << (BellmanFordsSolution(C, edges) ? "Arbitrage" : "Ok") << std::endl;
        std::cout << (FloidUorshillSolution(C, curs, g) ? "Arbitrage" : "Ok") << std::endl;
    }
}