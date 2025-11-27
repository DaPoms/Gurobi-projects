#include "gurobi_c++.h"
#include <iostream>
#include <fstream>
#include <filesystem> // I'm only using the scope operator for this one (not using namespace) as this is a new library I'm getting used to
#include <vector>
#include <string> //Need gap, best objective, time for excel
using namespace std;
//namespace fs = std::filesystem;

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
{
    
    //note, presolve on by default
    //formatting and creating a csv file following an excel format
    ofstream excel("coldstart.csv"); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
    excel << "Name:" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << '\n';
    //std::filesystem::path problems;
   
    std::filesystem::path problems{"C:/Users/Pomer/Desktop/Gurobi projects/Jooken_test/problemInstances"}; //Problem instances are provided by JorikJooken github: https://github.com/JorikJooken/knapsackProblemInstances 
    
    
    
    //GRBEnv env = GRBEnv(); //Stack version
    GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    //VERY IMPORTANT LESSON LEARNED:
    /* 
    The WLS variant (a new variant I am now using) of Gurobi licenses for the C++ API requires creating a GRB
     env with = GRBEnv(True) as we want to manually input the gurobi license (the non WLS version automates this process).
     Make sure to use getenv(varName) for security reasons, as having this license information would be a huge security issue.
    */
    (env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    (env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    (env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
    env.start();




    ifstream testFile;  
    vector<long long> values = {};
    vector<long long> weights = {};
    int count;
    long long capacity{-1};
        for (const auto& entry : std::filesystem::recursive_directory_iterator(problems)) //traverses every "entity" in the given folder
        {
            GRBModel model(&env);
            model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program 
            model.set(GRB_DoubleParam_TimeLimit, 60); //I believe you can actually change this with GBREnv to affect all models
            GRBLinExpr objective; //expression for maximizing values
            GRBLinExpr weightExpr;
            
            
            if(entry.path().filename() == "test.in") //Specifies what file we want to use that we find in any folder
            {
                //File reading + writing section
                testFile.open(entry.path());
                
                readFile(testFile, weights, values, count, capacity); //we assume the testFile is formatted properly (starts with count, then all the elements in the problem, and ends with the capacity of the knapsack)
                
                //Applying file info to a model + defining knapsack model
                GRBVar *x = model.addVars(count, GRB_BINARY); //Model will have to decide 1 to include and 0 to not include value to the knapsack

                for(int i{0}; i < count; i++) //when we test our set of elements, the total value is calculated as the summation of their value and whether they were picked for the knapsack (1,0)
                    objective += (values[i]) * x[i];
                model.setObjective(objective, GRB_MAXIMIZE); //Objective: We want to maximize our value that we get from values[i] * x[i]

                //Defines constraint of weight
                for(int i{0}; i < count; i++)
                    weightExpr +=  weights[i] * x[i];
                GRBConstr weightConstraint = model.addConstr(weightExpr <= (capacity), "capacity_constraint"); //defines relationship between total weight and capacity
                model.optimize(); //actually runs the optimization problem (uses the GRBLinExpr to test combinations).
                //profit needs to be manually calculated to prevent lossy conversion from double if I were to use GRB_DoubleAttr_ObjVal
                //Puts results in CSV file, only after calculating answer as a long long instead of the default double
                
             
                long long profit{0};
                 for(int i{0}; i < count; i++)
                {
                    if( x[i].get(GRB_DoubleAttr_X) >= 0.5) //Turns out x can only be a double, so we must use a bound rather than an exact value
                        profit += values[i];
                } 
                excel << entry.path().parent_path().filename() << "," << profit << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl;
                //excel << entry.path().parent_path().filename() << "," << profit << endl;

                //Resetting values before the next model   
                delete[] x; //I believe since x is from an api funtction that uses the heap, we need to delete it at the end to prevent memory leaks
                testFile.close(); // to move onto the next model file
                values.clear();
                weights.clear();
            }
        
        excel.close(); 
        }
    
}


