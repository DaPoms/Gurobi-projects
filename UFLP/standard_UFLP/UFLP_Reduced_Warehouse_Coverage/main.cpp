#include "gurobi_c++.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <random> // for randomizing shuffle()
#include <algorithm> // for std::sort()
using namespace std;
namespace fs = std::filesystem;


double COVERAGE_PROPORTION = 0.25;
//int fixedShuffleSeed = 172;


/* 
    This program models the uncapacitated facility location problem for use with Gurobi
    This is for the Reduced warehouse coverage version, which involves adding the constraint that customers cannot
    be serviced by just ANY warehouse to achieve there demand, rather a given percentage (that isn't 100%) can. (This test is for 75%, 50%, and 25% coverage)
*/

struct UFLPInstance
{
    vector<double> servicePrices;
    vector<double> fixedPrices;
    int facilityCount;
    int customerCount;
};

void readUFLP(string inputFileName, UFLPInstance& UFLP)
{
    string skipWord; // Just used to skip a >>
    vector<double> fixedPrices;
    vector<double> servicePrices;
    double fixedPrice;
    double servicePrice;
    ifstream file{inputFileName};
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


    ///// NEW constraint for a given customer, which warehouses can satisfy a given customer
    // Random variant (will make a method later)
    /* std::random_device rd; // random_device generates a random int that fits within 32 bits, more random than using the system clock
    unsigned int random_Seed = rd(); //unsigned required to fit larger size
    mt19937 generator(random_Seed);
    int canServiceCount = UFLProblem.facilityCount * COVERAGE_PROPORTION; // amount of warehouses that CAN service a given custonmer
    for(int c{0}; c < UFLProblem.customerCount; c++)
    {
        vector<bool> canService(UFLProblem.facilityCount, false);
        for(int i{0}; i < canServiceCount; i++)
            canService[i] = true;
        shuffle(canService.begin(), canService.end(), generator);

        for(int f{0}; f < UFLProblem.facilityCount; f++) 
            if(!canService[f])
                x[c][f].set(GRB_DoubleAttr_UB, 0.0); // found out about setting upper bound instead of adding a new constraint at https://docs.gurobi.com/projects/optimizer/en/current/concepts/attributes/examples.html 
        
    } */
   // Removing from top variant variant
   
   int cannotServiceCount = UFLProblem.facilityCount * (1 - COVERAGE_PROPORTION);
   targetPriceI = 0;
   for(int c{0}; c < UFLProblem.customerCount; c++)
    {
        vector<double> serviceCosts; //contains the service costs of all facilities for the c-th customer
        vector<int> facilityIndexes;
        for(int i{0}; i < UFLProblem.facilityCount; i++)  
        {
            facilityIndexes.push_back(i);
            serviceCosts.push_back(UFLProblem.servicePrices[targetPriceI++]);
        }
        // sorts so that the index of the most expensive to service warehouse for the cth customer is stored at the start of facilityIndexes (as its sorted from greatest to least)
        sort(facilityIndexes.begin(), facilityIndexes.end(), [&serviceCosts](int i, int j) 
        {
            return (serviceCosts[i] > serviceCosts[j]);
        }
        );

        
        // for(int i : facilityIndexes) // just for printing out highest to lowest service prices
        //    cout << UFLProblem.servicePrices[i + (c * UFLProblem.facilityCount)] << endl;
        //cout << endl; 
        
       
        for(int i{0}; i < cannotServiceCount; i++) 
            x[c][facilityIndexes[i]].set(GRB_DoubleAttr_UB, 0.0);
    }
   

    /////

        model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program  
        model.set(GRB_DoubleParam_TimeLimit, 3600); 
        //model.write("model.lp");

        model.read("cadizFineTune.prm");
        model.optimize();

        if(model.get(GRB_IntAttr_SolCount) > 0)
            excel << std::setprecision(4) << std::fixed << model.get(GRB_DoubleAttr_ObjVal) << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
        else // case of infeasible solution 
            excel << -1 << "," << model.get(GRB_DoubleAttr_Runtime) << endl; 


        //random removal output 
        /* 
        if(model.get(GRB_IntAttr_SolCount) > 0)
            excel << std::setprecision(4) << std::fixed << model.get(GRB_DoubleAttr_ObjVal) << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << "," << random_Seed << endl; 
        else // case of infeasible solution 
            excel << -1 << "," << model.get(GRB_DoubleAttr_Runtime) << "," << random_Seed << endl; 
        */
}

int main()
{
    //ofstream excel("UFLP_MT1000-2000.csv"); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
    ofstream excel("MT1_1000-2000_25p_ReducedVersTopRemoved.csv");
    excel << "Name" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << "," << "seed" << endl;

    GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    (env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    (env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    (env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
    env.start();

    //reading + solving
    fs::path problemFolderPath = "C:/Users/Pomer/Desktop/Gurobi projects/UFLP/standard_UFLP/problem_sets_(from_other_people)/Cadiz_1000-2000_MT1";
    int skipUpToCount{0}; // 0 = not skipping
    for(const fs::directory_entry& problemPath : fs::recursive_directory_iterator(problemFolderPath))
    {
        if(skipUpToCount) // skips all problems before and including the skipUpToCount-th problem
        {
            skipUpToCount--;
            cout << "skipped " << problemPath.path().filename() << "\n";
            continue; 
        }
        UFLPInstance UFLP;
        readUFLP(problemPath.path().string(), UFLP);
        excel << problemPath.path().filename().string() << ",";
        runGurobiUFLP(env, excel, UFLP);
    }

    return 0;
}


//NOTE nonrandom version (removing most expensive ones) would involve sorting by value,then marking them as not usable from their associated idx
