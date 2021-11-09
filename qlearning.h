#ifndef QLEARNING_TABLE_H
#define QLEARNING_TABLE_H
#include "tiles.h"
#include "ns3/double.h"
#include "ns3/random-variable-stream.h"

using namespace std;
using namespace ns3;

#define NDIM 2

class  QlearningTable 
{
public:
  QlearningTable(int actions, 
                 double learning_rate, 
                 double reward_decay, 
                 double e_greddy,
                 char cs_id,
                 int ntiling,
                 int ntiles,
                 double learning_decay,
                 double explore_decay,
                 double lambda
                );  

  int choose_action(double stateVec[]);

  int choose_best(double stateVec[]);

  int choose_random();

  void qlearning_update(double lastStateVec[], int last_action, double reward, double newStateVec[]);

  double update_sarsa_lambda_before(double lastStateVec[], int last_action, double reward);

  void   update_sarsa_lambda_after(double newStateVec[], int new_action, double TDerror);

  void update_sarsa_lambda_terminal(double TDerror);

  double update_q_lambda_before(double lastStateVec[], int last_action, double reward);

  void   update_q_lambda_after(double newStateVec[], int new_action, double TDerror);

  double calculate_action_value_q_estimate(double stateVec[], int act_id, int tiles_array[]);

  void set_parameter();
  int save_table();
  int load_table();
  void check_qtable();

private:
  int m_actions;
  double m_learning_rate;
  double m_reward_decay;
  double m_e_greedy;
  double m_lambda;      // lambda for eligibility trace decay

  int m_ntiling;
  int m_ntiles;
  std::vector<double> m_weights;
  std::vector<double> m_traces;

  double m_learning_decay;
  double m_explore_decay;

  std::string m_fname;

  Ptr<UniformRandomVariable> m_rand_probability;
};
#endif // QLEARNING_TABLE_H