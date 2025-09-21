#include "gurobi_c++.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
using namespace std;

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
    
    //NOTE: env GRB_DoubleParam_TimeLimit lets you limit the time spent optimizing, good for future tasks
    //std::filesystem::path problems = "C:/Users/Pomer/Desktop/Gurobi projects/Jooken_test/problemInstances"; //Problem instances are provided by JorikJooken github: https://github.com/JorikJooken/knapsackProblemInstances 
    //GRBEnv env = GRBEnv(); //Stack version
    GRBEnv *env = new GRBEnv(); //Heap version
    (*env).set(GRB_IntParam_Presolve, 0); //everything was being done instantly with pre-solve on   IMPORTANT
    (*env).set(GRB_StringParam_LogFile, "Jooken_test.log"); 
    GRBModel model(env);
    GRBLinExpr objective; //expression for maximizing values
    GRBLinExpr weightExpr;
    

    //File reading section
    ifstream testFile("test.in"); //MAKE SURE TO USE .CLEAR BEFORE NEXT FILE
    vector<long long> values = {};
    vector<long long> weights = {};
    int count;
    long long capacity{-1};
    readFile(testFile, weights, values, count, capacity); //we assume the testFile is formatted properly (starts with count, then all the elements in the problem, and ends with the capacity of the knapsack)
    //Applying file info to a model
    GRBVar *x = model.addVars(count, GRB_BINARY); //Model will have to decide 1 to include and 0 to not include value to the knapsack
    for(int i{0}; i < count; i++) //when we test our set of elements, the total value is calculated as the summation of their value and whether they were picked for the knapsack (1,0)
        objective += values[i] * x[i];
    model.setObjective(objective, GRB_MAXIMIZE); //Objective: We want to maximize our value that we get from values[i] * x[i]

    //Defines constraint of weight
    for(int i{0}; i < count; i++)
        weightExpr += weights[i] * x[i];
    model.addConstr(weightExpr <= capacity, "capacity_constraint"); //defines relationship between total weight and capacity
    model.optimize(); //actually runs the optimization problem.

}

