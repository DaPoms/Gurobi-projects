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
    std::filesystem::path problems{"C:/Users/Pomer/Desktop/Gurobi projects/Jooken_test/problemInstances"}; //Problem instances are provided by JorikJooken github: https://github.com/JorikJooken/knapsackProblemInstances 
    //NOTE: env GRB_DoubleParam_TimeLimit lets you limit the time spent optimizing, good for future tasks
    
    //GRBEnv env = GRBEnv(); //Stack version
    GRBEnv *env = new GRBEnv(); //Heap version (can change dynamically)
    (*env).set(GRB_IntParam_Presolve, 0); //everything was being done instantly with pre-solve on  
    
    //(*env).set(GRB_StringParam_LogFile, "Jooken_test.log"); 
    
    // GRBLinExpr objective; //expression for maximizing values
    // GRBLinExpr weightExpr;
    
        ifstream testFile;
        vector<long long> values = {};
        vector<long long> weights = {};
        int count;
        long long capacity{-1};
        for (const auto& entry : std::filesystem::recursive_directory_iterator(problems)) //traverses every "entity" in the given folder
        {
            GRBModel model(env);
            model.set(GRB_DoubleParam_MIPGap, 0.001); //What we deem optimal mipgap to terminate the program 
            //model.set(GRB_IntParam_PoolSolutions, 0);
            //model.set(GRB_IntParam_PoolSearchMode, 0);
            GRBLinExpr objective; //expression for maximizing values
            GRBLinExpr weightExpr;
            
            //File reading section
            if(entry.path().filename() == "test.in") //Specifies what file we want to use that we find in any folder
            {
                testFile.open(entry.path()); //MAKE SURE TO USE .CLEAR BEFORE NEXT FILE
                //testFile.open("test.in");
                //cout << "BEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEP: " << entry.path().filename() << "\n"; //OJGREIJGIEGJEJIG
                readFile(testFile, weights, values, count, capacity); //we assume the testFile is formatted properly (starts with count, then all the elements in the problem, and ends with the capacity of the knapsack)
                //Applying file info to a model + defining knapsack model
                GRBVar *x = model.addVars(count, GRB_BINARY); //Model will have to decide 1 to include and 0 to not include value to the knapsack
                for(int i{0}; i < count; i++) //when we test our set of elements, the total value is calculated as the summation of their value and whether they were picked for the knapsack (1,0)
                    objective += values[i] * x[i];
                model.setObjective(objective, GRB_MAXIMIZE); //Objective: We want to maximize our value that we get from values[i] * x[i]

                //Defines constraint of weight
                for(int i{0}; i < count; i++)
                    weightExpr += weights[i] * x[i];
                GRBConstr weightConstraint = model.addConstr(weightExpr <= capacity, "capacity_constraint"); //defines relationship between total weight and capacity

                model.optimize(); //actually runs the optimization problem.

                
                
                    std::cout << "Optimal value:WREKGJEGKREJRIJGI " << model.get(GRB_DoubleAttr_ObjVal) << std::endl;
                
                //We do not need to .remove objective for each model as it is overwritten by .setobjective()
                delete[] x; //I believe since x is from an api call that uses the heap, we need to delete it at the end to prevent memory leaks
                model.remove(*model.getVars()); 
                model.remove(*model.getConstrs());
                model.reset(); //Clears x */
                testFile.close(); // to move onto the next model file
                values.clear();
                weights.clear();
            }
        }
}
//.reset can be used for changing start behavior

//Goal: write to excel file automatically
