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
MDMKProblem formIntoProblem (double bestObjVal, vector<double>& line3ObjVals, vector<double>& line4CapacityLimits, vector<double>& line5DemandLimits, vector<vector<double>>& line6CandidateCapacityVals, vector<vector<double>>& line7CandidateDemandVals)
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


vector<vector<int>> runGurobiMDMKPScherer(GRBEnv& env, ofstream& excel, vector<MDMKProblem>& problems)
{
    int blockNum{1};
    vector<vector<int>> sols;
    /////////////////////////////////////// Test branch code!!!! (ONCE AGAIN, REMOVE THIS AFTER DONE THE TESTING)
    for(auto problem : problems)
    {

        if(blockNum != 18 && blockNum != 20 && blockNum != 25 && blockNum != 27 && blockNum != 35 && blockNum != 36 && blockNum != 44 && blockNum != 45 ) // only runs the hardest Scherer cases
        {
            blockNum++;
            continue;
        }
    /////////////////////////

        vector<GRBLinExpr> demandConstr;
        vector<GRBLinExpr> capacityConstr;
        GRBLinExpr objective;
        GRBModel model(env);
        
        
        
        model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program  
        model.set(GRB_DoubleParam_TimeLimit, 3600); 
        vector<GRBVar> x; //variable for if we include / not include item in knapsack
        model.set(GRB_IntParam_MIPFocus, 2);
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
                    demandExpr += problem.candidates[e].demandVal[i] * x[e]; // REMINDER: FOR NON CASE 1, edit demandVAL[0]   
            demandConstr.push_back(demandExpr);
        }
        for(int i{0}; i < demandConstr.size(); i++)
            model.addConstr(demandConstr[i] >= problem.knapsackDemandRequirementVals[i] );
      

        // model.write("testModel.lp"); //Insane new method I learned that helps a lot with debugging, outputs a file that visually shows what the model holds
         model.optimize();


        //Stores answer in sol
        vector<int> sol;
        if(model.get(GRB_IntAttr_SolCount) > 0)
            for(int i{0}; i < problem.candidates.size(); i++)
                sol.push_back(x[i].get(GRB_DoubleAttr_X));
        sols.push_back(sol);
        ///////////////////////

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

    return sols;
}

void runGurobiMDMKPSchererWarmSSIT(GRBEnv& env, ofstream& excel, vector<MDMKProblem>& problems)
{
    vector<vector<int>> warmSols = runGurobiMDMKPScherer(env, excel, problems);
    
    int blockNum{1};
    int warmStartI{0};
    /////////////////////////////////////// Test branch code!!!! (ONCE AGAIN, REMOVE THIS AFTER DONE THE TESTING)
    for(auto problem : problems)
    {
        vector<int> warmSol;
        if(blockNum != 18 && blockNum != 20 && blockNum != 25 && blockNum != 27 && blockNum != 35 && blockNum != 36 && blockNum != 44 && blockNum != 45 ) // only runs the hardest Scherer cases
        {
            blockNum++;
            continue;
        }
        else
        {
            warmSol = warmSols[warmStartI]; // Not the safest way, but the easiest way for warm start
            warmStartI++;
        }

    /////////////////////////

        vector<GRBLinExpr> demandConstr;
        vector<GRBLinExpr> capacityConstr;
        GRBLinExpr objective;
        GRBModel model(env);
        
        
        
        model.set(GRB_DoubleParam_MIPGap, 0.001); //What we deem optimal mipgap to terminate the program  
        model.set(GRB_DoubleParam_TimeLimit, 3600); 
        model.set(GRB_IntParam_MIPFocus, 2);
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

        // warm start 
        if(warmSol.size() != 0)
            for(int i{0}; i < problem.candidates.size(); i++)
                x[i].set(GRB_DoubleAttr_Start, warmSol[i]);

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

























bool isMixedProblem(vector<MDMKCandidate> candidates)
{
    for(MDMKCandidate c : candidates)
    {
        if(c.value < 0)
            return true;
    }
    return false;
}
 
//Fills a CSV with whether the problems are mixed or positive value coefficient probems
void storeProblemType(const vector<MDMKProblem>& problems, ofstream& excel)
{
    int blockNum = 1;
    for (MDMKProblem problem : problems)
    {
        vector<MDMKCandidate>& candidates = problem.candidates; // I somehow forgot the & is used to make references (too much java programming this semester! They automatically evaluate by reference, not by hard copy)
        bool isMixed = isMixedProblem(candidates);

        if(isMixed)
        {
            excel << blockNum << "," << "M" << endl;
        }
        else
        {
            excel << blockNum << "," << "P" << endl;
        }
        blockNum++;
    }
}


int main()
{
    ofstream excel("MDMKP_SchererHardestproblemsMipfocus2.csv"); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
    excel << "Name" << "," << "Best known Obj" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << endl;

    GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    (env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    (env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    (env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
    env.start();

    vector<MDMKProblem> problems = readSchererMDMKP("C:/Users/Pomer/Desktop/Gurobi projects/Scherer_MDMKP/MDMKP_Instances_Scherer.txt");
    //storeProblemType(problems, excel);
    runGurobiMDMKPSchererWarmSSIT(env, excel, problems);
    excel.close();

    return 0;
}
