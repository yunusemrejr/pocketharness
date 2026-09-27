// `pocket kit` operations: networking, SEO, data profiling and benchmarks.
#pragma once

#include <string>
#include <vector>

namespace pocket {

int kitPorts(const std::vector<std::string>& args);  // listening sockets + owning process
int kitWait(const std::vector<std::string>& args);   // block until a port/URL answers
int kitNet(const std::vector<std::string>& args);    // HTTP timing, TLS, headers
int kitSeo(const std::vector<std::string>& args);    // on-page SEO audit
int kitCsv(const std::vector<std::string>& args);    // dataset profile
int kitBench(const std::vector<std::string>& args);  // repeated timing with RSS

}  // namespace pocket
