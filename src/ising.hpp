#ifndef _ISING_
#define _ISING_
#include <cstdint>          // int64_t, etc
#include <vector>           // std::vector
#include <functional>       // std::multiplies
#include <numeric>          // std::partial_sum
#include <cmath>            // exp
#include "rng.hpp"          // RNG_ull, Rng_d
#include "cfg_types.hpp"    // num_digits, popcount
template <typename Cfg>
class Ising {
    using v_cfg = std::vector<Cfg>;
    using v_int64 = std::vector<int64_t>;
    using v_double = std::vector<double>;
    using vv_double = std::vector<v_double>;

    const v_int64 len;
    const v_int64 cumprod_len;
    const int64_t N;
    const int64_t d;
    const int64_t d_times_2;
    const v_cfg   masks;
    vv_double     acc_prob_lkp;
    double        kbt;
    double        beta;
    double        h;
    Cfg           cfg;
    int64_t       num_aligned_pairs;
    int64_t       num_ones;
    RNG_d<false>  rng;

    v_int64 get_cumprod_len() const {
      auto ans = len;
      ans.emplace(ans.begin(), 1LL);
      std::partial_sum(
                      ans.begin(), 
                      ans.end(), 
                      ans.begin(), 
                      std::multiplies<int64_t>());
      return ans;
    }
    v_cfg get_masks() const {
      v_cfg ans(N, Cfg(0));
      for(int64_t i = 0LL; i < N; i++) {
        Cfg mask = Cfg(0);
        for(int64_t j = 0LL; j < d_times_2; j++) {
          mask |= Cfg(1) << get_neighbour_index(i, j);
        }
        ans[i] = mask;
      }
      return ans;
    }
    int64_t get_neighbour_index(int64_t i, int64_t j) const {
      int64_t pm[] = {-1LL, 1LL};
      int64_t i_diff = pm[j % 2LL];
      int64_t crd_id = j / 2LL;
      int64_t crd_old = (i / cumprod_len[crd_id]) % len[crd_id];
      int64_t crd_new = (len[crd_id] + crd_old + i_diff) % len[crd_id];
      return i + (crd_new - crd_old) * cumprod_len[crd_id];
    }
    int64_t compute_num_aligned_pairs() const {
      int64_t num = 0LL;
      static constexpr Cfg one = Cfg(1);
      for(int64_t i = 0; i < N; i++) {
        int64_t n_nbr_ones_i = popcount(cfg & masks[i]);
        int64_t lkp[] = {d_times_2 - n_nbr_ones_i, n_nbr_ones_i};
        num += lkp[(cfg >> i) & one]; 
      }
      return num / 2;
    }
    void update_acc_prob_lkp() {
      int64_t n_aigned = -d_times_2;
      for(auto & vv : acc_prob_lkp) {
        int64_t ones_delta = -1;
        for(auto & val: vv) {
          val = exp(beta * (n_aigned + h * ones_delta));
          ones_delta += 2LL;
        }
        n_aigned++;
      }
    }
  public:
    Ising(v_int64 len_in, Cfg i_cfg = Cfg(0))
      : len(len_in)
      , cumprod_len(get_cumprod_len())
      , N(cumprod_len.back())
      , d(len_in.size())
      , d_times_2(2LL * d)
      , masks(get_masks())
      , acc_prob_lkp(vv_double(2LL * d_times_2 + 1LL, v_double(2LL, 0.)))
      {
        if(N > num_digits<Cfg>()) {
          std::cout << "Number of sites " << N 
                    << " is too great for " << num_digits<Cfg>()
                    << " bit representation."
                    << "\nYour should really throw an exception here.\n";
        }
        setkbt(1.);
        seth(0.);
        setcfg(i_cfg);
      }
    void setkbt(double val) { 
      kbt = val; 
      beta = 1. / val; 
      update_acc_prob_lkp(); 
    }  
    void seth(double val) { 
      h = val;
      update_acc_prob_lkp(); 
    }
    void setcfg(Cfg val) { 
      static constexpr Cfg zero = Cfg(0);
      static constexpr Cfg one = Cfg(1);
      if(val >= (one << N)) {
        cfg = (one << N) - one;
      } else if(val < zero) {
        cfg = zero;
      } else {
        cfg = val;
      }
      num_aligned_pairs = compute_num_aligned_pairs();
      num_ones = popcount(cfg);
    }
    void mcmove() {
      int64_t i = N * rng();
      static constexpr Cfg one = Cfg(1);
      Cfg cfg_new = cfg ^ (one << i);
      bool is_one = (cfg_new >> i) & one;

      int ones_delta = is_one ? 1:-1;
      int64_t n_nbr_ones = popcount(cfg_new & masks[i]);
      int64_t aligned_delta = ones_delta * (2LL * n_nbr_ones - d_times_2);

      if(rng() < acc_prob_lkp[d_times_2 + aligned_delta][is_one]) {
        cfg = cfg_new;
        num_ones += ones_delta;
        num_aligned_pairs += aligned_delta;
      }
    }
    int64_t get_num_ones() const { return num_ones; }
    int64_t get_num_aligned() const { return num_aligned_pairs; }
};
#endif