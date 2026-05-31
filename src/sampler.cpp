#include <vector>
#include <algorithm>
#include <random>
#include <cmath>

int sample_top_k(const std::vector<float>& logits, int k) {
    std::vector<std::pair<float,int>> v;
    for (int i=0;i<logits.size();i++) v.push_back({logits[i],i});
    
    // Clamp k
    if (k > (int)v.size()) k = (int)v.size();
    if (k <= 0) k = 1;

    std::partial_sort(v.begin(), v.begin()+k, v.end(),
        [](auto&a,auto&b){return a.first>b.first;});

    std::vector<float> probs(k);
    float sum=0;
    for(int i=0;i<k;i++){ probs[i]=expf(v[i].first); sum+=probs[i]; }
    for(float& p:probs)p/=sum;

    std::discrete_distribution<int> dist(probs.begin(), probs.end());
    static std::mt19937 rng{std::random_device{}()};
    return v[dist(rng)].second;
}

int sample_top_p(const std::vector<float>& logits, float p) {
    std::vector<std::pair<float,int>> v;
    for(int i=0;i<logits.size();i++) v.push_back({logits[i],i});
    std::sort(v.begin(),v.end(),
        [](auto&a,auto&b){return a.first>b.first;});

    float cum=0;
    std::vector<float> probs;
    std::vector<int> ids;
    for(auto& x:v){
        float pr=expf(x.first);
        probs.push_back(pr);
        ids.push_back(x.second);
        cum+=pr;
        if(cum>=p) break;
    }
    // Fallback if p is too small or something
    if (probs.empty()) {
        probs.push_back(1.0f);
        ids.push_back(v[0].second);
    } else {
        for(float& pr:probs) pr/=cum;
    }

    std::discrete_distribution<int> dist(probs.begin(),probs.end());
    static std::mt19937 rng{std::random_device{}()};
    
    // Safety check
    int idx = dist(rng);
    if (idx < 0 || idx >= ids.size()) return ids[0];
    return ids[idx];
}
