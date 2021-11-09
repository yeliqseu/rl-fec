#include <cmath>
#include "qlearning.h"

using namespace std;
using namespace ns3;



QlearningTable::QlearningTable(int actions,  
                               double learning_rate, 
                               double reward_decay, 
                               double e_greddy,
                               char cs_id,
                               int ntiling = 4,
                               int ntiles = 500,
                               double learning_decay = 0.995,
                               double explore_decay = 0.99,
                               double lambda = 0
                              )
{
  m_fname = std::string("table") + std::string(1,cs_id) + std::string("lambda") + std::to_string(lambda);
  m_actions = actions;
  m_learning_rate = learning_rate;
  m_reward_decay = reward_decay;
  m_e_greedy = e_greddy;
  m_lambda = lambda;

  m_rand_probability = CreateObject<UniformRandomVariable> ();
  m_rand_probability->SetAttribute ("Min", DoubleValue (0.0));
  m_rand_probability->SetAttribute ("Max", DoubleValue (1.0));

  m_ntiling = ntiling;
  m_ntiles = ntiles;
  m_weights = std::vector<double> (m_ntiles);
  m_traces  = std::vector<double> (m_ntiles);
  int exist = load_table();
  if(exist == 0) {
    cout << "file is not exist!" << endl;
  }

  m_learning_decay = learning_decay;  // not used
  m_explore_decay =  explore_decay;   // not used
}

int QlearningTable::load_table()
{
  int exist = 0;
  if( access( m_fname.c_str(), F_OK ) != -1 ) {
    exist = 1;
    FILE *fp = fopen(m_fname.c_str(), "r");
    for (uint32_t i=0; i<m_weights.size(); i++) {
        fscanf(fp, "%lf \t", &(m_weights[i]));
        fscanf(fp, "\n");
    }
  } else {
    exist = 0;
  }
  return exist;
}

int QlearningTable::save_table()
{
  FILE *f = fopen(m_fname.c_str(), "w");
  for (uint32_t i=0; i<m_weights.size(); i++) {
    fprintf(f, "%.4f\t", m_weights[i]);
    fprintf(f, "\n");
  }
  fclose(f);
  return 0;
}

int QlearningTable::choose_best(double stateVec[])
{ 
  int tiles_array[m_ntiling];
  double maxvalue = calculate_action_value_q_estimate(stateVec, 0, tiles_array);
  int best_action = 0;
  for (int i=1; i<m_actions; i++) {
    double result = calculate_action_value_q_estimate(stateVec, i, tiles_array);
    if (result > maxvalue) {
      maxvalue = result;
      best_action = i;
    }
  }
  std::cout
    << " choose greedy action " 
    << best_action << std::endl;
  return best_action;  
}

int QlearningTable::choose_random()
{
  int random_action = rand() % m_actions;
  std::cout
    << " choose random action " 
    << random_action << std::endl;
  return random_action;
}

int QlearningTable::choose_action(double stateVec[])
{
  double p = m_rand_probability->GetValue();
  int cur_action;
  if(p > m_e_greedy) {
    // judge 
    cur_action = choose_best (stateVec);
  }
  else {
    cur_action = choose_random();
  }
  return cur_action;
} 

double QlearningTable::calculate_action_value_q_estimate(double stateVec[], int act_id, int tiles_array[])
{
  int act[1] = { act_id };
  tiles(tiles_array, m_ntiling, m_ntiles, stateVec, NDIM, act, 1);
  double q_est = 0.0;
  for(int i=0; i<m_ntiling; i++) {
    q_est += m_weights[tiles_array[i]];
  }
  return q_est;
}

void QlearningTable::qlearning_update(double lastStateVec[], int last_action, double reward, double newStateVec[])
{
  int tiles_array[m_ntiling];
  double q_predict = calculate_action_value_q_estimate(lastStateVec, last_action, tiles_array);

  double max_q_next = -INFINITY;
  for(int i=0; i<m_actions; i++) 
    {
      double q_next = calculate_action_value_q_estimate(newStateVec, i, tiles_array);
      // std::cout << " q-value of action " << i << " : " << q_next << std::endl;
      max_q_next = q_next > max_q_next ? q_next : max_q_next;
    }
  
  // 重新获取tiles（之前的已被计算max时覆盖）
  int act[1] = { last_action };
  tiles(tiles_array, m_ntiling, m_ntiles, lastStateVec, NDIM, act, 1);
  for (int i=0; i<m_ntiling; i++) {
    m_weights[tiles_array[i]] += m_learning_rate * (reward + m_reward_decay * max_q_next - q_predict);  // linear approx., so gradient is simply the (0-1 sparse) feature vector
    if (isnan(m_weights[tiles_array[i]])) {
      std::cout << "Encounter NaN value" << std::endl;
    }
  }

}

double QlearningTable::update_sarsa_lambda_before(double lastStateVec[], int last_action, double reward)
{
  int tiles_array[m_ntiling];
  calculate_action_value_q_estimate(lastStateVec, last_action, tiles_array);
  double TDerror = reward;
  for (int i=0; i<m_ntiling; i++) {
    TDerror -= m_weights[tiles_array[i]];
    m_traces[tiles_array[i]] += 1;        // accumulating traces
  }
  return TDerror;
}


void QlearningTable::update_sarsa_lambda_after(double newStateVec[], int new_action, double TDerror)
{
  int tiles_array[m_ntiling];
  calculate_action_value_q_estimate(newStateVec, new_action, tiles_array);
  for (int i=0; i<m_ntiling; i++) {
    TDerror += m_reward_decay * m_weights[tiles_array[i]];
  }
  for (int i=0; i<m_ntiles; i++) {
    if (m_traces[i] != 0) {
      m_weights[i] += m_learning_rate * TDerror * m_traces[i];
      m_traces[i] *= m_reward_decay * m_lambda;
    }
  }
}

void QlearningTable::update_sarsa_lambda_terminal(double TDerror)
{
  for (int i=0; i<m_ntiles; i++) {
    if (m_traces[i] != 0) {
      m_weights[i] += m_learning_rate * TDerror * m_traces[i];
      m_traces[i] = 0;  // reset trace
    }
  }
}

double QlearningTable::update_q_lambda_before(double lastStateVec[], int last_action, double reward)
{
  int tiles_array[m_ntiling];
  calculate_action_value_q_estimate(lastStateVec, last_action, tiles_array);
  double TDerror = reward;
  for (int i=0; i<m_ntiling; i++) {
    TDerror -= m_weights[tiles_array[i]];
    m_traces[tiles_array[i]] += 1;        // accumulating traces
  }
  return TDerror;
}

void QlearningTable::update_q_lambda_after(double newStateVec[], int new_action, double TDerror)
{
  int tiles_array[m_ntiling];
  int best_action = choose_best(newStateVec);
  calculate_action_value_q_estimate(newStateVec, best_action, tiles_array);
  for (int i=0; i<m_ntiling; i++) {
    TDerror += m_reward_decay * m_weights[tiles_array[i]];
  }
  for (int i=0; i<m_ntiles; i++) {
    if (m_traces[i] != 0) {
      m_weights[i] += m_learning_rate * TDerror * m_traces[i];
      if(best_action == new_action) m_traces[i] *= m_reward_decay * m_lambda;
      else m_traces[i]=0;
    }
  }
}

void QlearningTable::set_parameter()
{
  //m_learning_rate = m_learning_rate * m_learning_decay;
  // m_e_greedy = min(m_e_greedy * m_explore_decay, 0.9);
  m_e_greedy = m_e_greedy * m_explore_decay;
}

void QlearningTable::check_qtable()
{
  for (int i=0; i<m_ntiles; i++) {
    if (m_traces[i] != 0)
      std::cout << "m_trace [" << i <<  "] is not zero" << std::endl;
  }
}
