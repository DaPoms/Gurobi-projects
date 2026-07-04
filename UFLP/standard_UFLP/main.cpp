#include "gurobi_c++.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
using namespace std;


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
        vector<GRBLinExpr> servicingConstr;
        vector<GRBLinExpr> constructConstr;
        GRBLinExpr objective; // obj is to minimize for UFLP
        GRBModel model(env);
        vector<vector<GRBVar>> x; //Decision variable for if warehouse serviced a given customer (vectors stored within x resemble warehouses, with these warehouses containing decision variables for each customer)
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
        }catch(GRBException e)
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

        model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program  ADD BACK ERIOJERIJGERJOGERJIOGERIOGREJIOGERJIOGERJIORJIOERGJIOERGERJIEGJI
        model.set(GRB_DoubleParam_TimeLimit, 5); 

        model.optimize();

        //long long profit{0};
        if(model.get(GRB_IntAttr_SolCount) > 0)
        {
          /*   for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
            {
                if( x[i].get(GRB_DoubleAttr_X) >= 0.5) //Turns out x can only be a double, so we must use a bound rather than an exact value
                    profit += caseNum.problemsByCase[0][i].value;
            }      */   
            //excel << "," <<  profit << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
            excel << "," << std::setprecision(4) << std::fixed <<  model.get(GRB_DoubleAttr_ObjVal) << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
        }
        else // case of infeasible solution 
        {
            //profit = -1;
            excel <<  -1 << "," << model.get(GRB_DoubleAttr_Runtime) << endl; 
        } 
}

int main()
{
    ofstream excel("ULFP.csv"); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
    excel << "Name" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << '\n';

    GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    (env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    (env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    (env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
    env.start();

    //reading 
   UFLPInstance UFLP;
    readUFLP("C:/Users/Pomer/Desktop/Gurobi projects/UFLP/standard_UFLP/problem_sets_(from_other_people)/cap71.txt", UFLP);
    //readMDMKP("mdmkp_ct8.txt", MDMKRawProblems);

    runGurobiUFLP(env, excel, UFLP);

    return 0;
}
