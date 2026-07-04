#include "gurobi_c++.h"
#include <iostream>
#include <fstream>
#include <filesystem> // I'm only using the scope operator for this one (not using namespace) as this is a new library I'm getting used to
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <string> //Need gap, best objective, time for excel
using namespace std;
//namespace fs = std::filesystem;




void greedyKnapsack(long long& capacity, long long currentWeight, vector<long long>& weights, unordered_map<int, pair<int, bool> >& candidates, vector<int>& answerElems)
{
    int i{0};
    while(i < int(candidates.size()) && currentWeight < capacity) //first phase of greedy
            {
                if(candidates[i].second == 1 && weights[candidates[i].first] + currentWeight <= capacity) //had to do this to make sure it checks past objects it cannot fit
                {
                    answerElems.push_back(candidates[i].first);
                    currentWeight += weights[candidates[i].first]; 
                    candidates[i].second = 0; //removes object from being a candidate, it is now in the bag
                    i--;
                }
                i++;
            }
}

void readFile(ifstream& problemFile, vector<long long>& weights, vector<long long>& values, int& count, long long& capacity)
{
    //First val is number of elems/count, last val is capacity of knapsack
    long long element;
    problemFile >> count;
    for(int i{0}; i < count; i++) //for every element
    {
        problemFile >> element; //ID (we skip as theres no use in knowing the id)
        problemFile >> element; //Value
        values.push_back(element);
        problemFile >> element; // Weight
        weights.push_back(element);
    }
    
    //Last value in the problem file will be the knapsack capacity
    problemFile >> capacity;
}

int main()
{ //note, presolve on by default
    //formatting and creating a csv file following an excel format
    ofstream excel("GreedyWarm.csv"); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
    excel << "Name:" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << '\n';
    std::filesystem::path problems{"C:/Users/Pomer/Desktop/Gurobi projects/Jooken_test/problemInstances"}; //Problem instances are provided by JorikJooken github: https://github.com/JorikJooken/knapsackProblemInstances 
    //GRBEnv env = GRBEnv(); //Stack version
    GRBEnv *env = new GRBEnv(); //Heap version (can change dynamically)
    ifstream testFile;  
    vector<long long> values = {};
    vector<long long> weights = {};
    int count;
    long long capacity{-1};
        for (const auto& entry : std::filesystem::recursive_directory_iterator(problems)) //traverses every "entity" in the given folder
        {
            GRBModel model(env);
            model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program 
            model.set(GRB_DoubleParam_TimeLimit, 60); //I believe you can actually change this with GBREnv to affect all models
            GRBLinExpr objective; //expression for maximizing values
            GRBLinExpr weightExpr;
            
            
            if(entry.path().filename() == "test.in") //Specifies what file we want to use that we find in any folder
            {

               



                //File reading + writing section
                testFile.open(entry.path());
                
                readFile(testFile, weights, values, count, capacity); //we assume the testFile is formatted properly (starts with count, then all the elements in the problem, and ends with the capacity of the knapsack)
                
                ///// WARM START SECTION /////
                vector<int> bestRatioIndexesRemaining; //index 0 has the highest ratio, last is the worst. Only contains elements NOT in our knapsack
                for(int i{0}; i < count; i++)
                    bestRatioIndexesRemaining.push_back(i);
                sort(bestRatioIndexesRemaining.begin(), bestRatioIndexesRemaining.end(), [&values, &weights](int i, int j) 
                {
                    return (double(values[i]) / weights[i]) > (double(values[j]) / weights[j]);
                } 
                );
                unordered_map<int, pair<int, bool> > candidates; //when bool = 1, element is still a candidate
                for(int i{0}; i < bestRatioIndexesRemaining.size(); i++)
                    candidates[i] = pair(bestRatioIndexesRemaining[i], 1);
                vector<int> answerElems;
                greedyKnapsack(capacity, 0, weights, candidates, answerElems);

                ///////////////////////

                //Applying file info to a model + defining knapsack model
                //GRBVar *x = model.addVars(count, GRB_BINARY); //Model will have to decide 1 to include and 0 to not include value to the knapsack
                vector<GRBVar> x;
                for(int i{0}; i < candidates.size(); i++)
                    x.push_back(model.addVar(0.0, 1.0, 0.0, GRB_BINARY)); // params are as follows: (lower bound, upper bound, starting val, type)


                for(int i{0}; i < count; i++) //when we test our set of elements, the total value is calculated as the summation of their value and whether they were picked for the knapsack (1,0)
                    objective += (values[i]) * x[i];
                model.setObjective(objective, GRB_MAXIMIZE); //Objective: We want to maximize our value that we get from values[i] * x[i]

                //Defines constraint of weight
                for(int i{0}; i < count; i++)
                    weightExpr +=  weights[i] * x[i];
                GRBConstr weightConstraint = model.addConstr(weightExpr <= (capacity), "capacity_constraint"); //defines relationship between total weight and capacity

                /// FEEDS WARM START ANSWER INTO MODEL ///
                for(int i{0}; i < candidates.size(); i++)
                {
                    if(candidates[i].second == 1) // == 1 means it wasn't picked in our answer
                        x[i].set(GRB_DoubleAttr_Start, 0.0);
                    else // case of element being in our knapsack
                        x[i].set(GRB_DoubleAttr_Start, 1.0);
                }


                //model.update(); // VERY IMPORTANT! ONLY WAY TO CHECK VALUES BEFORE RUNNING OPTIMIZE IS TO .UPDATE() before
               /*  for(int i{0}; i < candidates.size(); i++)
                {
                    try{
                    cout << x[i].get(GRB_DoubleAttr_Start) << '\n';
                    } catch (GRBException& e)
                    {
                        cout << "Code: " << e.getErrorCode() << endl;
                        cout << "error: " << e.getMessage() << endl; 
                    }
                }
                 */
                //////////////////////////////////////////
                model.optimize(); //actually runs the optimization problem (uses the GRBLinExpr to test combinations).
                //profit needs to be manually calculated to prevent lossy conversion from double if I were to use GRB_DoubleAttr_ObjVal
                //Puts results in CSV file, only after calculating answer as a long long instead of the default double
                
                

                long long profit{0};
                 for(int i{0}; i < count; i++)
                {
                    if( x[i].get(GRB_DoubleAttr_X) >= 0.5) //Turns out x can only be a double, so we must use a bound rather than an exact value
                        profit += values[i];
                } 
                // excel << entry.path().parent_path().filename() << "," << model.get(GRB_DoubleAttr_ObjVal) << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << "\n";
                excel << entry.path().parent_path().filename() << "," << profit << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl;

                //Resetting values before the next model   
                x.clear(); //I believe since x is from an api funtction that uses the heap, we need to delete it at the end to prevent memory leaks
                testFile.close(); // to move onto the next model file
                values.clear();
                weights.clear();
            }
        }
        excel.close(); 
}



