//Hexaly Imports
#include "optimizer/hexalyoptimizer.h"
#include "modeler/hexalymodeler.h"
using namespace hexaly;

//Gurobi import
//#include "gurobi_c++.h"

//CPLEX import
//#include <ilcplex/ilocplex.h>


//CP-SAT imports + alias'
/* #include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/cp_model.pb.h"

using operations_research::sat::CpModelBuilder;
using operations_research::sat::BoolVar;
using operations_research::sat::LinearExpr;
using operations_research::sat::SatParameters;
using operations_research::sat::CpSolverResponse;
using operations_research::sat::CpSolverStatus;
using operations_research::sat::SolveWithParameters;
 */

 //Mpsolver imports + alias
 /* 
#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "ortools/base/init_google.h"
#include "ortools/linear_solver/linear_solver.h"
using namespace operations_research;
 */
//general imports
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <algorithm>
using namespace std;
namespace fs = std::filesystem;

vector<vector<int>> readWarmStartsCustomerToFacilityMapping(string warmStartFilePath)
{
    vector<vector<int>> warmStarts;
    string s{""};
    int facilityIdxChosenByCustomer;

    ifstream fstream{warmStartFilePath};
    while(getline(fstream, s, '[') && s != "\n") // s != "\n" is needed to check for EOF since I overwrote getline to note use eof, "[" instead
    {

        vector<int> warmStart;
        char comma{','};
        while(comma == ',')
        {
            fstream >> facilityIdxChosenByCustomer;
            fstream >> comma;
            warmStart.push_back(facilityIdxChosenByCustomer);
        }
        warmStarts.push_back(warmStart);
    }
    return warmStarts;
}

vector<vector<int>> readWarmStartsFacilityOpened(string folderPath)
{
    vector<vector<int>> warmStarts;
    string s{""};
    int facilityIdxChosenByCustomer;

    
    for(const fs::directory_entry& warmStartFile : fs::recursive_directory_iterator(folderPath))
    {
        vector<int> warmStart;
        char comma{','};
        ifstream fstream{warmStartFile.path().string()};
        getline(fstream, s, '['); // gets fstream up to the solution
        while(comma == ',')
        {
            fstream >> facilityIdxChosenByCustomer;
            fstream >> comma;
            warmStart.push_back(facilityIdxChosenByCustomer);
        }
        warmStarts.push_back(warmStart);
    }
    return warmStarts;
}

/* 
    This program models the uncapacitated facility location problem for use with Gurobi
*/

struct UFLPInstance
{
    vector<double> servicePrices;
    vector<double> fixedPrices;
    int facilityCount;
    int customerCount;
};


//Supports Ghosh and MED problem format
/* 
    This format is specified by https://algo.rwth-aachen.de/ufllib/index.shtml#simp as:
    "The first line consists of 'FILE: ' and the name of the file. 
    The next line contains n, m and a 0. The subsequent n lines consist of the number of the facility,
    the opening cost, and the connetion cost to the cities."
*/
void readUFLPSimpleForm(string inputFileName, UFLPInstance& UFLP)
{
    string skipWord; // Just used to skip a >>
    vector<double> fixedPrices;
    
    double fixedPrice;
    double servicePrice;
    ifstream file{inputFileName};
    if(!file)
    {
        cerr << "Problem encountered in file reading";
        exit(EXIT_FAILURE);
    }
    int facilityCount, customerCount;
    file >> facilityCount >> customerCount >> skipWord;
    vector<vector<double>> servicePrices(customerCount);
    for(int i{0}; i < facilityCount; i++) // follows Beasley's format
    {
        file >> skipWord;
        fixedPrices.push_back((file >> fixedPrice, fixedPrice));
        for(int c{0}; c < customerCount; c++)
        {
            servicePrices[c].push_back((file >> servicePrice, servicePrice));
        }
    }

/*     for(int c{0}; c < customerCount; c++)
    {
        file >> skipWord; // skipped here is the demand value, which is exclusive to capacitated problems, not uncapacitated
        for(int i{0}; i < facilityCount; i++) // follows Beasley's format
        {
            file >> servicePrice;
            servicePrices.push_back(servicePrice);
        }
    } */

    vector<double> flattenedServicePrices;
    for(vector<double> serviceCostsForCustomer : servicePrices) // serviceCostsForCustomer is the service cost for each facility for a singular customer
        for(double d : serviceCostsForCustomer)
            flattenedServicePrices.push_back(d);
    UFLP.customerCount = customerCount;
    UFLP.facilityCount = facilityCount;
    UFLP.servicePrices = flattenedServicePrices; // Stored simply as every customerCount of items are attributed to a customer, so the 1st customer's servicing prices for 15 facilities would be indexes 0-14.
    UFLP.fixedPrices = fixedPrices;
}

//Works with Beasley and M*, as they share similar format that can be interpreted by this
// This reads files that follow the "ORLIB-Cap" format specified by https://algo.rwth-aachen.de/ufllib/index.shtml#simp as:
/* 
    "The format allows to store instances for the uncapacitated and capacitated facility location problem. 
    The first line of a file consists n and m. The next n lines are the opening cost and the capacity of each facility. 
    The following numbers for the cities are the demand and the connections to all facilities. For each city there is one line with the demand, 
    and one line with connection costs to all factilities.""
*/


void readUFLPBeasleyForm(string inputFileName, UFLPInstance& UFLP)
{
    string skipWord; // Just used to skip a >>
    vector<double> fixedPrices;
    vector<double> servicePrices;
    double fixedPrice;
    double servicePrice;
    ifstream file{inputFileName};
    if(!file)
    {
        cerr << "Problem encountered in file reading";
        exit(EXIT_FAILURE);
    }
    int facilityCount, customerCount;
    file >> facilityCount >> customerCount; // make sure to skip capacitated question parts
    for(int i{0}; i < facilityCount; i++) // follows Beasley's format
    {
        file >> skipWord >> fixedPrice; //skipped here is normally capacity value
        fixedPrices.push_back(fixedPrice);
    }

    for(int c{0}; c < customerCount; c++)
    {
        file >> skipWord; // skipped here is the demand value, which is exclusive to capacitated problems, not uncapacitated
        for(int i{0}; i < facilityCount; i++) // follows Beasley's format
        {
            file >> servicePrice;
            servicePrices.push_back(servicePrice);
        }
    }

    UFLP.customerCount = customerCount;
    UFLP.facilityCount = facilityCount;
    UFLP.servicePrices = servicePrices; // Stored simply as every customerCount of items are attributed to a customer, so the 1st customer's servicing prices for 15 facilities would be indexes 0-14.
    UFLP.fixedPrices = fixedPrices;
}

/* 
void runGurobiUFLP(GRBEnv& env, ofstream& excel, UFLPInstance& UFLProblem, const vector<int>& warmStart = {})
{
        GRBLinExpr objective; // obj is to minimize for UFLP
        GRBModel model(env);
        vector<vector<GRBVar>> x; //Decision variable for if warehouse serviced a given customer (vectors stored within x resemble customers, with these customer vectors containing decision variables for each warehouse)
        vector<GRBVar> y; // Decision variable for if warehouse was opened or not
//////////////////// objective value definition ///////////////

        for(int i{0}; i < UFLProblem.customerCount; i++)//initializes inner vectors of variable x, customers
            x.push_back(vector<GRBVar>());
        // servicing customer / not 
        for(int i{0}; i < UFLProblem.customerCount; i++) //only ordered this way to play around better with my servicePrices variable, which is made in customer order, not facility
            for(int c{0}; c < UFLProblem.facilityCount; c++)
                x[i].push_back(model.addVar(0.0, 1.0, 0.0, GRB_BINARY));
        // facility opened/unopened    
        for(int i{0}; i < UFLProblem.facilityCount; i++) 
            y.push_back(model.addVar(0.0, 1.0, 0.0, GRB_BINARY));

        int targetPriceI{0};
        for(int i{0}; i < UFLProblem.customerCount; i++) 
            for(int f{0}; f < UFLProblem.facilityCount; f++) 
                objective += UFLProblem.servicePrices[targetPriceI++] * x[i][f]; // ith customer for fth facilities 
        for(int f{0}; f < UFLProblem.facilityCount; f++) 
                objective += UFLProblem.fixedPrices[f] * y[f];

        model.setObjective(objective, GRB_MINIMIZE);

//////////////////// Constraints ///////////////
     // obj constraint (ensures that all customers are serviced by exactly 1 warehouse)
    for(int i{0}; i < UFLProblem.customerCount; i++)
    {
        GRBLinExpr satisfactionExpr; // Learned that this type is required for using GRBVars for constraint expressions
        for(int f{0}; f < UFLProblem.facilityCount; f++) 
            satisfactionExpr += x[i][f]; //note var "aij" is not included in beasley's version (all warehouses can satisfy any given customer), but likely this will have to be added here in the future
        try{
         model.addConstr(satisfactionExpr == 1);
        } catch(GRBException e)
        {
            cout << e.getMessage();
        }
    }  
    
    // constraint for validating that only open facilities can service customers (as in those with yi = 1)
    for(int f{0}; f < UFLProblem.facilityCount; f++) 
    {
        for(int i{0}; i < UFLProblem.customerCount; i++) 
            model.addConstr(x[i][f] <= y[f]);
    }

        model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program  
        //model.set(GRB_DoubleParam_TuneTimeLimit,86400 * 2); // total tune time
        model.set(GRB_DoubleParam_TimeLimit, 3600); //runs each trial 3 times
        model.set(GRB_IntParam_SolutionLimit, 1); // FOR PRINTING WARM STARTS, DELETE WHEN DONE TODO
        //model.set(GRB_IntParam_TuneCriterion,1); // NEW, focuses on MIPGAP
        //model.set(GRB_IntParam_TuneTrials, 3); 
        //model.read("cadizFineTune.prm");
        //model.read("Tripoli_Ghosh_gs500c_3Tune.prm"); // Tuning settings

        //tuning (with MS1)
        //model.tune();
        //model.getTuneResult(1); //0 is also the baseline, 1 is for best case
        //model.write("Ghosh_gs500c_3Tune.prm");
        //exit(0);


        // load warm start, only in terms of the facilities to be opened
        // NEW WARM START APPROACH
         for(int i{0}; i < warmStart.size(); i++) //only ordered this way to play around better with my servicePrices variable, which is made in customer order, not facility
            x[i][warmStart[i]].set(GRB_DoubleAttr_Start, 1.0);

    //     OLD WARM START APPROACH
       // if(warmStart.size() != 0)
       // {
        //    for(int i{0}; i < y.size(); i++)
      //      {
    //            y[i].set(GRB_DoubleAttr_Start, 0.0);
  //          }
//
         //   for(int openedFacility : warmStart)
       //     {
     //           y[openedFacility - 1].set(GRB_DoubleAttr_Start, 1.0);
   //         }
 //       }
 
        //
        model.optimize();
        //long long profit{0};
        if(model.get(GRB_IntAttr_SolCount) > 0)
        {
            //excel << "," <<  profit << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
            excel << "," << std::setprecision(4) << std::fixed <<  model.get(GRB_DoubleAttr_ObjVal) << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
            // output warm start of each customers decided on facility
          
            excel << '[';
            for(int c{0}; c < UFLProblem.customerCount; c++) //only ordered this way to play around better with my servicePrices variable, which is made in customer order, not facility
            {
                for(int f{0}; f < UFLProblem.facilityCount; f++)
                {
                    if(x[c][f].get(GRB_DoubleAttr_X) == 1.0)
                    {
                        excel << "," << f << " ";
                        break;
                    }

                }
                
            }
            excel << ']' << endl;
        }
        else // case of infeasible solution 
        {
            //profit = -1;
            excel <<  -1 << "," << model.get(GRB_DoubleAttr_Runtime) << endl; 
        } 
} 
 */

// Converts a flat vector that contains every facility count of elems attributed to the customer (so first facilityCount of elems belongs to the first customer, and so on...) to a 2d vector where each vector is a facility, and all the elements are the service price of each customer for that facility
vector<vector<double>> convertFlatCustViewToFacility(const UFLPInstance& UFLProblem)
{
    vector<double> flatCustViewList = UFLProblem.servicePrices;
    vector<vector<double>> ans(UFLProblem.facilityCount);
    for(vector<double>& facility : ans)
        facility.resize(UFLProblem.customerCount);
    int customerIdx{0};
    for(int i{0}; i < flatCustViewList.size(); i++) // does not error check if customer does not have all facility service costs
    {
        ans[i % UFLProblem.facilityCount][i / UFLProblem.facilityCount] = flatCustViewList[i];
    }
    return ans;
}

// HEXALY IMPLEMENTATION WAS MADE WITH SIGNIFICANT INSPIRATION FROM: https://www.hexaly.com/templates/capacitated-facility-location-problem-cflp
void runHexalyUFLP(ofstream& excel, UFLPInstance& UFLProblem, vector<int>& warmStart)
{
        HexalyOptimizer optimizer; 
        HxModel model = optimizer.getModel();

        // Decision var
        vector<HxExpression> facilityAssignments(UFLProblem.facilityCount); // holds decisions, with vector containing each faciilty, and the contents of the vector being a set of which customers are using the specific facility 
        
        // Defining decision var
        for (int f{0}; f < UFLProblem.facilityCount; f++) 
        {
            facilityAssignments[f] = model.setVar(UFLProblem.customerCount); // this creates a set to represent which customers are satisfied for a given facility (the set only contains customers of that facility)
        }
        // Enforcing customer utilizes only 1 facility and that all customers have their "demand" satisfied
        model.constraint(model.partition(facilityAssignments.begin(), facilityAssignments.end())); // partition returns true ONLY IF each customer is serviced (as denoted by .setVar() using customerCount, so each customer is represented with a unique number from 0 to customerCount - 1), and specifically by exactly ONE facility, not multiple, thus this enforces customer->facility relationship.
        
        // defining service price
        HxExpression allocationPrice = model.array();
        vector<vector<double>> servicePriceFacilityView = convertFlatCustViewToFacility(UFLProblem); // Flat list is in chunks of every facilityCount being all of the given customers service costs, starting at 0. This converts it to a clearer form where each facility contains the service costs each customer faces, starting with customer 0
        for (int f = 0; f < UFLProblem.facilityCount; ++f) 
        {
            allocationPrice.addOperand(model.array(servicePriceFacilityView[f].begin(), servicePriceFacilityView[f].end()));
        }
        // defining cost (obj val)
        vector<HxExpression> cost(UFLProblem.facilityCount); // Objective var
        for (int f = 0; f < UFLProblem.facilityCount; ++f) 
        {
                HxExpression facility = facilityAssignments[f];
                HxExpression size = model.count(facility);

                // Cost (allocation price + opening price)
                HxExpression costLambda =
                    model.createLambdaFunction([&](HxExpression i) { return model.at(allocationPrice, f, i); });
                cost[f] = model.sum(facility, costLambda) + (size > 0) * UFLProblem.fixedPrices[f];
        }
        
        HxExpression totalCost = model.sum(cost.begin(), cost.end());
        model.minimize(totalCost);
        model.close();
    // All ABOVE is HEXALY template

    optimizer.getParam().setGapLimit(0.0001);
    optimizer.getParam().setTimeLimit(3600); 
    // load warm start, only in terms of the facilities to be opened

    if(warmStart.size() != 0)
    {
        for(int i{0}; i < UFLProblem.facilityCount; i++)
        {
            facilityAssignments[i].getCollectionValue().clear(); // ensures all sets are empty (all warehouses are unopened)
        }

        for(int i{0}; i < warmStart.size(); i++)
        {
            facilityAssignments[warmStart[i]].getCollectionValue().add(i);
        }

    }
 
    auto start = chrono::high_resolution_clock::now();
    optimizer.solve();
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> solveDuration = end - start;
    
    HxSolution s = optimizer.getSolution();

    //long long profit{0};
    if(s.getStatus() == SS_Feasible ||s.getStatus() == SS_Optimal )
    {
        excel << "," << std::setprecision(4) << std::fixed <<  totalCost.getDoubleValue() << "," << solveDuration.count() << "," << s.getObjectiveGap(0) << endl; 
    }
    else // case of infeasible solution 
    {
        excel << "," << -1 << "," << optimizer.getStatistics().getRunningTime() << endl;
    }
}


 /* 
 void runCplexUFLP(ofstream& excel, UFLPInstance& UFLProblem, const vector<int>& warmStart = vector<int>())
 {        
        
        IloEnv env;
        IloModel model(env);
        IloNumExpr objective(env); // obj is to minimize for UFLP
        vector<vector<IloBoolVar>> x; //Decision variable for if warehouse serviced a given customer (vectors stored within x resemble customers, with these customer vectors containing decision variables for each warehouse)
        vector<IloBoolVar> y; // Decision variable for if warehouse was opened or not
//////////////////// objective value definition ///////////////

        for(int i{0}; i < UFLProblem.customerCount; i++)//initializes inner vectors of variable x
            x.push_back(vector<IloBoolVar>());
        // servicing customer / not 
        for(int i{0}; i < UFLProblem.customerCount; i++) //only ordered this way to play around better with my servicePrices variable, which is made in customer order, not facility
            for(int c{0}; c < UFLProblem.facilityCount; c++)
                x[i].push_back(IloBoolVar(env));
        // facility opened/unopened    
        for(int i{0}; i < UFLProblem.facilityCount; i++) 
            y.push_back(IloBoolVar(env));

        int targetPriceI{0};
        for(int i{0}; i < UFLProblem.customerCount; i++) 
            for(int f{0}; f < UFLProblem.facilityCount; f++) 
                objective += UFLProblem.servicePrices[targetPriceI++] * x[i][f]; // ith customer for fth facilities 
        for(int f{0}; f < UFLProblem.facilityCount; f++) 
                objective += UFLProblem.fixedPrices[f] * y[f];

        model.add(IloMinimize(env, objective));

//////////////////// Constraints ///////////////
     // obj constraint (ensures that all customers are serviced by exactly 1 warehouse)
    for(int i{0}; i < UFLProblem.customerCount; i++)
    {
        IloNumExpr satisfactionExpr(env); // Learned that this type is required for using GRBVars for constraint expressions
        for(int f{0}; f < UFLProblem.facilityCount; f++) 
            satisfactionExpr += x[i][f]; //note var "aij" is not included in beasley's version (all warehouses can satisfy any given customer), but likely this will have to be added here in the future
         model.add(satisfactionExpr == 1);
    }  
    
    // constraint for validating that only open facilities can service customers (as in those with yi = 1)
    for(int f{0}; f < UFLProblem.facilityCount; f++) 
    {
        for(int i{0}; i < UFLProblem.customerCount; i++) 
            model.add(x[i][f] <= y[f]);
    }
        IloCplex cplex(model); 
        cplex.setParam(IloCplex::Param::TimeLimit, 3600);
        cplex.setParam(IloCplex::Param::MIP::Tolerances::MIPGap, 0.0001);
        // below are settings to help with out of memory errors
        //cplex.setParam(IloCplex::Param::Emphasis::Memory, true);
        //cplex.setParam(IloCplex::Param::Threads, 6); // no crash with 2 threads default settings

        // Below are only needed for extremely large problems
        // cplex.setParam(IloCplex::Param::Parallel, 1); 
       // cplex.setParam(IloCplex::Param::WorkMem, 10240);
       // cplex.setParam(IloCplex::Param::MIP::Strategy::File, 3); // Allows offloading RAM memory to SSD if exceeded     
      //  cplex.setParam(IloCplex::Param::MIP::Limits::TreeMemory, 15000); // Search tree stop condition
     //   cplex.setParam(IloCplex::Param::Threads, 2);  // next try 4 (curr one is 1)

     // warm start
        if(warmStart.size() != 0)
        {
            IloNumVarArray serviceDecisionWarmVars{env};
            for(int i{0}; i < UFLProblem.customerCount; i++) //only ordered this way to play around better with my servicePrices variable, which is made in customer order, not facility
                for(int c{0}; c < UFLProblem.facilityCount; c++)
                    serviceDecisionWarmVars.add(x[i][c]);
            IloNumArray serviceDecisionWarmVals(env);
            for(int i{0}; i < UFLProblem.customerCount; i++) //only ordered this way to play around better with my servicePrices variable, which is made in customer order, not facility
                for(int c{0}; c < UFLProblem.facilityCount; c++)
                    serviceDecisionWarmVals.add(c == warmStart[i] ? 1.0 : 0.0);
            cplex.addMIPStart(serviceDecisionWarmVars, serviceDecisionWarmVals);
        }
 
        double startTime = cplex.getCplexTime();
        IloBool status = cplex.solve();
        double runTime = cplex.getCplexTime() - startTime;

        //long long profit{0};
        if(status)
        {
            //excel << "," <<  profit << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
            excel << "," << std::setprecision(4) << std::fixed <<  cplex.getObjValue() << "," << runTime << "," << cplex.getMIPRelativeGap() << endl; 
        }
        else // case of infeasible solution 
        {
            //profit = -1;
            excel << "," << -1 << "," << runTime << endl; 
        } 
        env.end(); // Must manually clean up for CPLEX, unlike Gurobi
}  
 
 */
/* 
void runCpSatUFLP(ofstream& excel, UFLPInstance& UFLProblem)
{

        int decimalToIntScaling = 10000; // 1 for MED and GHOSH, 10000 for M*, Beasley

        CpModelBuilder model;
        LinearExpr objective; // obj is to minimize for UFLP
        vector<vector<BoolVar>> x; //Decision variable for if warehouse serviced a given customer (vectors stored within x resemble customers, with these customer vectors containing decision variables for each warehouse)
        vector<BoolVar> y; // Decision variable for if warehouse was opened or not
//////////////////// objective value definition ///////////////

        for(int i{0}; i < UFLProblem.customerCount; i++)//initializes inner vectors of variable x
            x.push_back(vector<BoolVar>());
        // servicing customer / not 
        for(int i{0}; i < UFLProblem.customerCount; i++) //only ordered this way to play around better with my servicePrices variable, which is made in customer order, not facility
            for(int c{0}; c < UFLProblem.facilityCount; c++)
                x[i].push_back(model.NewBoolVar());
        // facility opened/unopened    
        for(int i{0}; i < UFLProblem.facilityCount; i++) 
            y.push_back(model.NewBoolVar());

        int targetPriceI{0};
        for(int i{0}; i < UFLProblem.customerCount; i++) 
            for(int f{0}; f < UFLProblem.facilityCount; f++) 
            {
                int64_t servicePrice = round(UFLProblem.servicePrices[targetPriceI++] * decimalToIntScaling);
                objective += servicePrice * x[i][f]; // ith customer for fth facilities 
            }
        for(int f{0}; f < UFLProblem.facilityCount; f++) 
        {  
            int64_t fixedPrice = round(UFLProblem.fixedPrices[f] * decimalToIntScaling);
            objective += fixedPrice * y[f];
        }

        model.Minimize(objective);

//////////////////// Constraints ///////////////
     // obj constraint (ensures that all customers are serviced by exactly 1 warehouse)
    for(int i{0}; i < UFLProblem.customerCount; i++)
    {
        LinearExpr satisfactionExpr; // Learned that this type is required for using GRBVars for constraint expressions
        for(int f{0}; f < UFLProblem.facilityCount; f++) 
            satisfactionExpr += x[i][f]; //note var "aij" is not included in beasley's version (all warehouses can satisfy any given customer), but likely this will have to be added here in the future
         model.AddEquality(satisfactionExpr, 1);
    }  
    
    // constraint for validating that only open facilities can service customers (as in those with yi = 1)
    for(int f{0}; f < UFLProblem.facilityCount; f++) 
    {
        for(int i{0}; i < UFLProblem.customerCount; i++) 
            model.AddLessOrEqual(x[i][f], y[f]);
    }

    operations_research::sat::SatParameters p;
    p.set_max_time_in_seconds(3600);
    p.set_relative_gap_limit(0.0001);
    p.set_num_workers(6);

    const CpSolverResponse result = SolveWithParameters(model.Build(), p);
    //long long profit{0};
    if(result.status() == CpSolverStatus::FEASIBLE || result.status() == CpSolverStatus::OPTIMAL) 
    {

        //excel << "," <<  profit << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
        excel << "," << std::setprecision(4) << std::fixed <<  result.objective_value() / decimalToIntScaling << "," << result.wall_time() << "," << (std::abs(result.best_objective_bound() - result.objective_value())) / result.objective_value() << endl; 
    } // FOR DECIMAL CASE MIGHt HAVE TO CAST TO DOUBLE
    else // case of infeasible solution 
    {
        //profit = -1;
        excel <<  -1 << "," << result.wall_time() << endl; 
    } 
} 
 */
/* 
void runMpSolver(ofstream& excel, UFLPInstance& UFLProblem)
{
        int decimalToIntScaling = 1; // 1 for MED and GHOSH, 10000 for M*, Beasley

       std::unique_ptr<MPSolver> model(MPSolver::CreateSolver("highs"));
       //std::unique_ptr<MPSolver> model(MPSolver::CreateSolver("scip"));
        if (!model) 
        {
            LOG(WARNING) << "solver unavailable.";
            return;
        }
        
        MPObjective* objective = model->MutableObjective(); // obj is to minimize for UFLP
        vector<vector<MPVariable*>> x; //Decision variable for if warehouse serviced a given customer (vectors stored within x resemble customers, with these customer vectors containing decision variables for each warehouse)
        vector<MPVariable*> y; // Decision variable for if warehouse was opened or not
//////////////////// objective value definition ///////////////
    
        for(int i{0}; i < UFLProblem.customerCount; i++)//initializes inner vectors of variable x
            x.push_back(vector<MPVariable*>());
        // servicing customer / not 
        for(int i{0}; i < UFLProblem.customerCount; i++) //only ordered this way to play around better with my servicePrices variable, which is made in customer order, not facility
            for(int c{0}; c < UFLProblem.facilityCount; c++)
                x[i].push_back(model->MakeBoolVar("x_" + to_string(i) + "_" + to_string(c)));
        // facility opened/unopened    
        for(int i{0}; i < UFLProblem.facilityCount; i++) 
            y.push_back(model->MakeBoolVar("y " + to_string(i)));

        int targetPriceI{0};
        for(int i{0}; i < UFLProblem.customerCount; i++) 
            for(int f{0}; f < UFLProblem.facilityCount; f++) 
                objective->SetCoefficient(x[i][f], UFLProblem.servicePrices[targetPriceI++]); // ith customer for fth facilities 
        for(int f{0}; f < UFLProblem.facilityCount; f++) 
                objective->SetCoefficient(y[f], UFLProblem.fixedPrices[f]);

        objective->SetMinimization();

//////////////////// Constraints ///////////////
     // obj constraint (ensures that all customers are serviced by exactly 1 warehouse)
    for(int i{0}; i < UFLProblem.customerCount; i++)
    {
        LinearExpr satisfactionExpr; // Learned that this type is required for using GRBVars for constraint expressions
        for(int f{0}; f < UFLProblem.facilityCount; f++) 
            satisfactionExpr += x[i][f]; //note var "aij" is not included in beasley's version (all warehouses can satisfy any given customer), but likely this will have to be added here in the future
         model->MakeRowConstraint(satisfactionExpr == 1);
    }  
    
    // constraint for validating that only open facilities can service customers (as in those with yi = 1)
    for(int f{0}; f < UFLProblem.facilityCount; f++) 
    {
        for(int i{0}; i < UFLProblem.customerCount; i++) 
            //model.AddLessOrEqual(x[i][f], y[f]);
            model->MakeRowConstraint(LinearExpr(x[i][f]) <= LinearExpr(y[f]));
    }

    //model->SetTimeLimit(absl::Seconds(3600));
    model->SetSolverSpecificParametersAsString("time_limit=3600.0"); //Highs time limit
    model->SetSolverSpecificParametersAsString("mip_rel_gap=0.0001");
    MPSolverParameters p;
    model->EnableOutput();
    p.SetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP, 0.0001);

    const MPSolver::ResultStatus result = model->Solve();

    if(result == MPSolver::OPTIMAL || result == MPSolver::FEASIBLE) 
    {

        //excel << "," <<  profit << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
        excel << "," << std::setprecision(4) << std::fixed <<  objective->Value() / decimalToIntScaling << "," << ((double)model->wall_time()) / 1000.0 << "," << (std::abs(objective->BestBound() - objective->Value())) / objective->Value() << endl; 
    } // FOR DECIMAL CASE MIGHt HAVE TO CAST TO DOUBLE
    else // case of infeasible solution 
    {
        //profit = -1;
        excel <<  -1 << "," << ((double)model->wall_time()) / 1000.0 << endl; 
    } 
} 
 */
int main()
{
    ofstream excel("Hexaly_GloverWARMSTART_ghoshREMAININGPROBLEMS_3600s_untuned.csv");
    //ofstream excel("Ghosh_WarmStartNEWAPPROACHTest.csv");
    excel << "Name" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << '\n';
    
    /* GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    (env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    (env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    (env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
    env.start(); */
   
    //reading + solving
    fs::path problemFolderPath = "C:/Users/Pomer/Desktop/Gurobi projects/problems/UFLP/KoerkelGhosh-sym/allghosh";
    //fs::path problemFolderPath = "C:/Users/Pomer/Desktop/Gurobi projects/problems/UFLP/kmedian/allkmedian";
    //fs::path problemFolderPath = "C:/Users/Pomer/Desktop/Gurobi projects/problems/UFLP/beasleyUFLP/all_beasley_problems";
    //fs::path problemFolderPath = "C:/Users/Pomer/Desktop/Gurobi projects/problems/UFLP/M/allmstar";
    
    //BAD warm start approach
    //vector<vector<int>> warmStarts = readWarmStartsFacilityOpened("C:/Users/Pomer/Desktop/Gurobi projects/UFLP/warmStarts/Glover/ghosh/");
    //vector<vector<int>> warmStarts = readWarmStartsFacilityOpened("C:/Users/Pomer/Desktop/Gurobi projects/UFLP/warmStarts/Glover/MED"); 

    vector<vector<int>> warmStartByCustomer = readWarmStartsCustomerToFacilityMapping("C:/Users/Pomer/Desktop/Gurobi projects/UFLP/warmStarts/Glover/ghosh/Ghosh_WarmStartCustomerToFacilityMapping.csv");
    //vector<vector<int>> warmStartByCustomer = readWarmStartsCustomerToFacilityMapping("C:/Users/Pomer/Desktop/Gurobi projects/UFLP/warmStarts/Glover/MED/MED_WarmStartCustomerToFacilityMapping.csv");

    int i = 0;
    for(const fs::directory_entry& problemPath : fs::recursive_directory_iterator(problemFolderPath))
    {
        if(i < 33)
        {
            i++;
            continue;
        }
        UFLPInstance UFLP;
        //readUFLPBeasleyForm(problemPath.path().string(), UFLP); // Used for M* and Beasley datasets 
        readUFLPSimpleForm(problemPath.path().string(), UFLP); // Used for Ghosh and MED datasets
        excel << problemPath.path().filename().stem().string();
        std::cout << "Started\n";
        //runGurobiUFLP(env, excel, UFLP, warmStartByCustomer[i++]);
        runHexalyUFLP(excel, UFLP, warmStartByCustomer[i++]);
        //runCplexUFLP(excel, UFLP, warmStartByCustomer[i++]);
        //runCpSatUFLP(excel, UFLP);
        //runMpSolver(excel, UFLP);
        std::cout << "Ended\n";
        

    }

    return 0;
}

// CPSAT cannot use and floats, so must multiply it up
// Hexaly I need to look into set notation for faculty -> customer / customer -> faculty pairings
// CPLEX should be fine to directly translate Gurobi -> CPLEX
// WARNING: HEXALY currently does not include decimal place of runtime   


// CPSAT only accepts INTEGER values, so I must values up by the decimal place and cast to int



//TODO: GHOSH with hexaly
// todo: gs500c-3 cplex
