#include "gurobi_c++.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <stack>
#include <iomanip>
using namespace std;

struct MDMKCandidate
{
    vector<double> capacityVal; // How much capacity this candidate takes up for each capacity constraint
    vector<double> demandVal; //How much demand value this candidate contributes
    double value; // "Cost": how much this item is worth (either +/- depending on the case)
};

struct MDMKProblem
{
    vector <MDMKCandidate> candidates; //candidates 
    vector<double> knapsackCapacityVals;
    vector<double> knapsackDemandRequirementVals; 
    double bestObjVal;
};

///Scherer specific code //////////////////////////////////////////lear
MDMKProblem formIntoProblem (double bestObjVal, vector<double> line3ObjVals, vector<double> line4CapacityLimits, vector<double> line5DemandLimits, vector<vector<double>> line6CandidateCapacityVals, vector<vector<double>> line7CandidateDemandVals)
{
    MDMKProblem ans; 
    vector<MDMKCandidate> candidates;
    int candidateCount = line3ObjVals.size();
    int capacityCount = line6CandidateCapacityVals.size();
    int demandCount = line7CandidateDemandVals.size();
    for(int i{0}; i < candidateCount; i++)
    {
        MDMKCandidate candidate;
        candidate.value = line3ObjVals[i];
        for(int c{0}; c < capacityCount; c++)
            candidate.capacityVal.push_back(line6CandidateCapacityVals[c][i]);
        
        for(int d{0}; d < demandCount; d++)
            candidate.demandVal.push_back(line7CandidateDemandVals[d][i]);
        candidates.push_back(candidate);
    }
    ans.bestObjVal = bestObjVal;
    ans.knapsackCapacityVals = line4CapacityLimits;
    ans.knapsackDemandRequirementVals = line5DemandLimits;
    ans.candidates = candidates;
    return ans;
}

vector<double> read1BracketFromNestedSchererStringStream(stringstream& line)
{
    vector<double> ans; //Scherer's has at most 2 layers of vectors, so this accounts for it. If something like obj value, it will just be a vector with 1 vector in it
    string lineString; 
    double d;
    char trashConsumer; // At the end, the comma consumer actually consumes the right bracket (which is fine)
    line >> trashConsumer; // Consumes first bracket
    while(true) // Scherer's format has the bracket at the end of the line
    {
        ans.push_back((line >> d >> trashConsumer, d));
        if (trashConsumer == ']') 
        {
            line >> trashConsumer; // consumes comma, but at the end, will consume the closing right bracket of the entire statement
            break;
        }
    }
    return ans;
}
//reads and collects all doubles stored within the bracket specified via param
vector<vector<double>> readLineNestedBracketOfScherer(ifstream& file)
{
    vector<vector<double>> ans; //Scherer's has at most 2 layers of vectors, so this accounts for it. If something like obj value, it will just be a vector with 1 vector in it
    stack<char> brackets; //keeps track of closure, goal is to pop off the first bracket we push on
    string lineString;
    char trashConsumer;
    file >> trashConsumer; // Consumes first bracket
    stringstream line{(getline(file, lineString), lineString)};
    while(line.peek() != -1) // -1 = EOF
        ans.push_back(read1BracketFromNestedSchererStringStream(line));
    return ans;
}

vector<double> read1BracketOfScherer(ifstream& file)
{
    vector<double> ans; //Scherer's has at most 2 layers of vectors, so this accounts for it. If something like obj value, it will just be a vector with 1 vector in it
    string lineString; 
    double d;
    char trashConsumer; // At the end, the comma consumer actually consumes the right bracket (which is fine)
    getline(file, lineString); //string stream allows easier traversal
    stringstream lineStream{lineString};
    lineStream >> trashConsumer; // Consumes first bracket
    while(true) // Scherer's format has the bracket at the end of the line
    {
        ans.push_back((lineStream >> d >> trashConsumer, d));
        if (trashConsumer == ']') break;
    }
    return ans;
}

//Returns all problems
vector<MDMKProblem> readSchererMDMKP(string fileName) 
{
    vector<MDMKProblem> ans;

    //Fills ans ONE AT A TIME (much better for adapting to new formats)
    
    ifstream file{fileName};
    while(file.peek() != -1)
    {
        long candidateCount, leConstraintCount, geConstraintCount; //leConstraints means <= constraints (also known as capacity constraints)
        double bestObjVal;
        vector<double> line1 = read1BracketOfScherer(file); //Line 1 contains block num AND best (found) obj val
        long testProblemNum = line1[0];
        bestObjVal = line1[1];
        vector<double> line2 = read1BracketOfScherer(file); // This is candidate count, capacity count, demand count
        candidateCount = line2[0];
        leConstraintCount = line2[1]; //TODO: Need to use these for defining problem
        geConstraintCount = line2[2];
        vector<double> line3ObjVals = read1BracketOfScherer(file); // This is candidate count, capacity count, demand count
        vector<double> line4CapacityLimits = read1BracketOfScherer(file); 
        vector<double> line5DemandLimits = read1BracketOfScherer(file); 
        vector<vector<double>> line6CandidateCapacityVals = readLineNestedBracketOfScherer(file);
        vector<vector<double>> line7CandidateDemandVals = readLineNestedBracketOfScherer(file);
        ans.push_back(formIntoProblem(bestObjVal, line3ObjVals, line4CapacityLimits, line5DemandLimits, line6CandidateCapacityVals, line7CandidateDemandVals));
    }
    file.close();
    return ans;

}

///////////////////////////////////


void runGurobiMDMKPScherer(GRBEnv& env, ofstream& excel, vector<MDMKProblem>& problems)
{
    int blockNum{1};
    /////////////////////////////////////// Test branch code!!!! (ONCE AGAIN, REMOVE THIS AFTER DONE THE TESTING)
    for(auto problem : problems)
    {
    /////////////////////////

        vector<GRBLinExpr> demandConstr;
        vector<GRBLinExpr> capacityConstr;
        GRBLinExpr objective;
        GRBModel model(env);
        
        
        
        model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program  ADD BACK ERIOJERIJGERJOGERJIOGERIOGREJIOGERJIOGERJIORJIOERGJIOERGERJIEGJI
        model.set(GRB_DoubleParam_TimeLimit, 3600); 
        vector<GRBVar> x; //variable for if we include / not include item in knapsack
//////////////////// objective value definition ///////////////
        for(int i{0}; i < problem.candidates.size(); i++) 
            x.push_back(model.addVar(0.0, 1.0, 0.0, GRB_BINARY)); 
       
        for(int i{0}; i < problem.candidates.size(); i++)
                objective += problem.candidates[i].value * x[i]; 
        model.setObjective(objective, GRB_MAXIMIZE);
//////////////////// capacity constraint ///////////////
        for(int i{0}; i < problem.knapsackCapacityVals.size(); i++) 
        {
            GRBLinExpr capacityExpr;
           
            for(int e{0}; e < problem.candidates.size(); e++)
                capacityExpr += problem.candidates[e].capacityVal[i] * x[e]; // REMINDER: FOR NON CASE 1, edit demandVAL[0]
                
            capacityConstr.push_back(capacityExpr);
        }
        for(int i{0}; i < capacityConstr.size(); i++)
            model.addConstr(capacityConstr[i] <= problem.knapsackCapacityVals[i] );
       
//////////////////// demand constraint ///////////////
        for(int i{0}; i < problem.candidates[0].demandVal.size(); i++)  // NEED TO CHECK CASE VAL REIJEIOGJIGJERJIGOERGIOREGJIERGJIOERGJIOERGJIOGJOERGJO
        {
            GRBLinExpr demandExpr;
            for(int e{0}; e < problem.candidates.size(); e++)
            {
                    //demandExpr += case1.candidates[e].demandVal[dCount] * x[i]; // REMINDER: FOR NON CASE 1, edit demandVAL[0]
                    demandExpr += problem.candidates[e].demandVal[i] * x[e]; // REMINDER: FOR NON CASE 1, edit demandVAL[0]
                
            }
            demandConstr.push_back(demandExpr);
        }
        for(int i{0}; i < demandConstr.size(); i++)
            model.addConstr(demandConstr[i] >= problem.knapsackDemandRequirementVals[i] );
      
        

        

        // model.write("testModel.lp"); //Insane new method I learned that helps a lot with debugging, outputs a file that visually shows what the model holds
         model.optimize();

        double profit{0};
        if(model.get(GRB_IntAttr_SolCount) > 0)
        {

            for(int i{0}; i < problem.candidates.size(); i++)
            {
                if( x[i].get(GRB_DoubleAttr_X) >= 0.5) //Turns out x can only be a double, so we must use a bound rather than an exact value
                    profit += problem.candidates[i].value;
            }        
            excel << setprecision(15) << to_string(blockNum) + "_" + to_string(problem.candidates.size()) + "_" + to_string(problem.knapsackCapacityVals.size()) + "_" + to_string(problem.knapsackDemandRequirementVals.size()) << "," << problem.bestObjVal << "," <<  profit << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
      
        }
        else // case of infeasible solution 
        {
            profit = -1;
            excel << setprecision(15) << to_string(blockNum) + "_" + to_string(problem.candidates.size()) + "_" + to_string(problem.knapsackCapacityVals.size()) + "_" + to_string(problem.knapsackDemandRequirementVals.size()) << ","<< problem.bestObjVal << ","  <<  profit << "," << model.get(GRB_DoubleAttr_Runtime) << endl; 
        }
        blockNum++;






    }
}
 


int main()
{
    ofstream excel("MDMKP_Scherer.csv"); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
    excel << "Name" << "," << "Best known Obj" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << endl;

    GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    (env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    (env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    (env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
    env.start();

    vector<MDMKProblem> problems = readSchererMDMKP("C:/Users/Pomer/Desktop/Gurobi projects/Scherer_MDMKP/MDMKP_Instances_Scherer.txt");

    runGurobiMDMKPScherer(env, excel, problems);
    excel.close();

    return 0;
}
