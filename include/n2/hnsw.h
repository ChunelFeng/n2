// Copyright 2017 Kakao Corp. <http://www.kakaocorp.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "hnsw_build.h"
#include "hnsw_model.h"
#include "hnsw_search.h"
#include "hnsw_searcher_pool.h"

namespace n2 {

class Hnsw {
public:
    Hnsw();
    Hnsw(int dim, std::string metric="angular");
    Hnsw(const Hnsw& other);
    Hnsw(Hnsw&& other) noexcept;
    ~Hnsw();

    Hnsw& operator=(const Hnsw& other);
    Hnsw& operator=(Hnsw&& other) noexcept;

    ////////////////////////////////////////////
    // Build
    void AddData(const std::vector<float>& data);
    void SetConfigs(const std::vector<std::pair<std::string, std::string>>& configs);
    void Build(int m=-1, int max_m0=-1, int ef_construction=-1, int n_threads=-1, float mult=-1, 
               NeighborSelectingPolicy neighbor_selecting=NeighborSelectingPolicy::HEURISTIC, 
               GraphPostProcessing graph_merging=GraphPostProcessing::SKIP, bool ensure_k=false);
    void Fit();

    ////////////////////////////////////////////
    // Model
    bool SaveModel(const std::string& fname) const;
    bool LoadModel(const std::string& fname, const bool use_mmap=true);
    void UnloadModel();
 
    ////////////////////////////////////////////
    // Search 
    inline void SearchByVector(const std::vector<float>& qvec, size_t k, size_t ef_search,
                               std::vector<int>& result) {
        searcher_->SearchByVector(qvec, k, ef_search, ensure_k_, result);
    }
    inline void SearchByVector(const std::vector<float>& qvec, size_t k, size_t ef_search,
                               std::vector<std::pair<int, float>>& result) {
        searcher_->SearchByVector(qvec, k, ef_search, ensure_k_, result);
    }
    inline void SearchById(int id, size_t k, size_t ef_search, std::vector<int>& result) {
        searcher_->SearchById(id, k, ef_search, ensure_k_, result);
    }
    inline void SearchById(int id, size_t k, size_t ef_search, std::vector<std::pair<int, float>>& result) {
        searcher_->SearchById(id, k, ef_search, ensure_k_, result);
    }

    inline void BatchSearchByVectors(const std::vector<std::vector<float>>& qvecs, size_t k, 
                                     size_t ef_search, int n_threads, std::vector<std::vector<int>>& results) {
        BatchSearchByVectors_(qvecs, k, ef_search, n_threads, results);
    }
    inline void BatchSearchByVectors(const std::vector<std::vector<float>>& qvecs, size_t k, 
                                     size_t ef_search, int n_threads, 
                                     std::vector<std::vector<std::pair<int, float>>>& results) {
        BatchSearchByVectors_(qvecs, k, ef_search, n_threads, results);
    }
    inline void BatchSearchByIds(const std::vector<int> ids, size_t k, size_t ef_search, int n_threads,
                                 std::vector<std::vector<int>>& results) {
        BatchSearchByIds_(ids, k, ef_search, n_threads, results);
    }
    inline void BatchSearchByIds(const std::vector<int> ids, size_t k, size_t ef_search, int n_threads,
                                 std::vector<std::vector<std::pair<int, float>>>& results) {
        BatchSearchByIds_(ids, k, ef_search, n_threads, results);
    }

    ////////////////////////////////////////////
    // Build(Misc)
    void PrintDegreeDist() const;
    void PrintConfigs() const;

private:
    void InitSearcherAndSearcherPool_();

    template<typename ResultType>
    void BatchSearchByVectors_(const std::vector<std::vector<float>>& qvecs, size_t k, 
                               size_t ef_search, int n_threads, ResultType& results) {
        results.resize(qvecs.size());
        #pragma omp parallel num_threads(n_threads)
        {
            #pragma omp for schedule(guided)
            for (size_t i = 0; i < qvecs.size(); ++i) {
                auto s = searcher_pool_->GetInstanceFromPool();
                s->SearchByVector(qvecs[i], k, ef_search, ensure_k_, results[i]);
                searcher_pool_->ReturnInstanceToPool(s);
            }
        }
    }

    template<typename ResultType>
    void BatchSearchByIds_(const std::vector<int> ids, size_t k, size_t ef_search, int n_threads, ResultType& results) {
        results.resize(ids.size());
        #pragma omp parallel num_threads(n_threads)
        {
            #pragma omp for schedule(guided)
            for (size_t i = 0; i < ids.size(); ++i) {
                auto s = searcher_pool_->GetInstanceFromPool();
                s->SearchById(ids[i], k, ef_search, ensure_k_, results[i]);
                searcher_pool_->ReturnInstanceToPool(s);
            }
        }
    }

private:
    std::unique_ptr<HnswBuild> builder_;
    std::shared_ptr<const HnswModel> model_;
    std::shared_ptr<HnswSearch> searcher_;                  // for single-thread search
    std::unique_ptr<HnswSearcherPool> searcher_pool_;       // for multi-threads batch search

    size_t data_dim_;
    DistanceKind metric_;
    bool ensure_k_ = false;
};

} // namespace n2
