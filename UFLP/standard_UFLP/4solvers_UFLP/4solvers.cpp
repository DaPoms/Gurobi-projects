#include "optimizer/hexalyoptimizer.h"
#include "modeler/hexalymodeler.h"

//#include "gurobi_c++.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <filesystem>
using namespace std;
using namespace hexaly;
namespace fs = std::filesystem;


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
void runGurobiUFLP(GRBEnv& env, ofstream& excel, UFLPInstance& UFLProblem)
{
        GRBLinExpr objective; // obj is to minimize for UFLP
        GRBModel model(env);
        vector<vector<GRBVar>> x; //Decision variable for if warehouse serviced a given customer (vectors stored within x resemble customers, with these customer vectors containing decision variables for each warehouse)
        vector<GRBVar> y; // Decision variable for if warehouse was opened or not
//////////////////// objective value definition ///////////////

        for(int i{0}; i < UFLProblem.customerCount; i++)//initializes inner vectors of variable x
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
        model.set(GRB_DoubleParam_TimeLimit, 3600); 
        //model.read("cadizFineTune.prm");
        model.optimize();
        //long long profit{0};
        if(model.get(GRB_IntAttr_SolCount) > 0)
        {
            //excel << "," <<  profit << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
            excel << "," << std::setprecision(4) << std::fixed <<  model.get(GRB_DoubleAttr_ObjVal) << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
        }
        else // case of infeasible solution 
        {
            //profit = -1;
            excel <<  -1 << "," << model.get(GRB_DoubleAttr_Runtime) << endl; 
        } 
} */

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
void runHexalyUFLP(ofstream& excel, UFLPInstance& UFLProblem)
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

int main()
{
    //ofstream excel("UFLP_MT1000-2000.csv"); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
    ofstream excel("HEXALY_UFLP_MSTAR_problems_3600s_untuned.csv");
    excel << "Name" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << '\n';

   /*  GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    (env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    (env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    (env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
    env.start(); */

    //reading + solving
    fs::path problemFolderPath = "C:/Users/Pomer/Desktop/Gurobi projects/problems/UFLP/BeasleyUFLP/all_beasley_problems";
    for(const fs::directory_entry& problemPath : fs::recursive_directory_iterator(problemFolderPath))
    {
        UFLPInstance UFLP;
        readUFLPBeasleyForm(problemPath.path().string(), UFLP); // Used for M* and Beasley datasets
        //readUFLPSimpleForm(problemPath.path().string(), UFLP); // Used for Ghosh and MED datasets
        excel << problemPath.path().filename().stem().string();
        //runGurobiUFLP(env, excel, UFLP);
        runHexalyUFLP(excel, UFLP);
    }

    // TODO write scripts for reading Ghosh and MED (CLSA,B,C), and also versions for CP-SAT, CPLEX, and Hexaly

    return 0;
}

// CPSAT cannot use and floats, so must multiply it up
// Hexaly I need to look into set notation for faculty -> customer / customer -> faculty pairings
// CPLEX should be fine to directly translate Gurobi -> CPLEX
// WARNING: HEXALY currently does not include decimal place of runtime   


// CPSAT only accepts INTEGER values, so I must values up by the decimal place and cast to int
