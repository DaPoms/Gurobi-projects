#include "gurobi_c++.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <filesystem>
#include <regex>
using namespace std;
namespace fs = std::filesystem;




/* 
NOTE: While this approach is less efficient than the fileread.cpp version, I decided on 
this approach as it is more readable, and the heuristics will take long anyways, so a few extra 
seconds of processing the data read from the file means nothing in the end
 */
//reads one block of data from MDMKP .txt file (one block = either <=, >= constraints or cost coefficient vals)
/* 
Function name: readAttributeOfMDMKP
Params:
    ifstream& file: The file we want to read from, should be "pointing" to the start of the attribute we want to read 
    vector<vector<long>>& candidateCapacityAtrributes: Where we will store each candidates attribute for each constraint (reminder that there are multiple capacity constraints and demand constraints in each problem)
    bool IsConstraint: if true, will read an additional line that will be the capacity/demand max/min(s) depending on the attribute we are reading. (<= constraint is capacity / max, >= constraint is demand/min)


*/

class MDMKRawProblem // each MDMKRawProblem is actually a set of 6 problems in 1 entity, but processing must be done for that to formm thus the "raw" name
{
    private:
        vector<vector<long>> candidateCapacityAtrributes;
        vector<vector<long>> candidateDemandAtrributes;
        vector<vector<long>> candidateCostAtrributes;
        vector<long> knapsackCapacityVals;
        vector<long> knapsackDemandRequirementVals;
    public:
        vector<vector<long>>& getcandidateCapacityAtrributes() { return candidateCapacityAtrributes; } 
        vector<vector<long>>& getcandidateDemandAtrributes() { return candidateDemandAtrributes; }
        vector<vector<long>>& getCandidateValue() { return candidateCostAtrributes; }
        vector<long>& getknapsackCapacityVals() { return knapsackCapacityVals; }
        vector<long>& getknapsackDemandRequirementVals() { return knapsackDemandRequirementVals; }

};

struct MDMKCandidate
{
    vector<long> capacityVal; // How much capacity this candidate takes up for each capacity constraint
    vector<long> demandVal; //How much demand value this candidate contributes
    long value; // "Cost": how much this item is worth (either +/- depending on the case)
};

struct problemSet
{
    vector < vector <MDMKCandidate> > problemsByCase;
    vector<long> knapsackCapacityVals;
    vector<long> knapsackDemandRequirementVals; 
};


//Formats MKMDProblem into their 6 respective cases, in accordance to the brunel paper. Here is a definition of each case:
/* 
    Note that "q" is the number of demand constraints (>=) and "m" is the number of capacity (<=) constraints
    These constraints are picked in order as they show up. EX: when q=1, it means only the first demand constraint is considered
    Positive cost/value cases
    Case 1: q = 1
    Case 2: q = m/2
    Case 3: q = m
    Mixed cost/value cases
    Case 4: q = 1
    Case 5: q = m/2
    Case 6: q = m
*/

void separateCandidatesByCases(MDMKRawProblem& problem, problemSet& candidatesByCase) 
{
    candidatesByCase.problemsByCase.resize(1); // new thing learned! Can be used to easily create uninitialized values inside of the vector (or reshape vector to fit this function's usage)
    vector<vector<long>> MDMKCapacityAttributes = problem.getcandidateCapacityAtrributes(); 
    vector<vector<long>> MDMKDemandAttributes = problem.getcandidateDemandAtrributes();
    vector<vector<long>> MDMKValue = problem.getCandidateValue();
    long capacityVarsCount = MDMKCapacityAttributes.size(); //Shortcut to finding the amount of capacity variables (dimensions) for the problem
    long candidateCount = MDMKCapacityAttributes[0].size();//This is a way to find the # of candidates we have without having to pass directly (not too efficient, but less params = simpler). Only works if problem is not empty
 
    for(int i{0}; i < candidateCount; i++)
    {
        MDMKCandidate candidate;
        for(int c{0}; c < capacityVarsCount; c++)
            candidate.capacityVal.push_back(MDMKCapacityAttributes[c][i]);
        
        candidate.value = MDMKValue[0][i];
        for(int currDemand{0}; currDemand < capacityVarsCount; currDemand++) //stops at case 6 (where same # of demands as capacity constraints)
            candidate.demandVal.push_back(MDMKDemandAttributes[currDemand][i]);
              
        candidatesByCase.problemsByCase[0].push_back(candidate);     
        candidate.demandVal.clear();///
          
    }
}

void RawProblemsToCases(vector<MDMKRawProblem>& problems, vector<problemSet>& cases)
{
    problemSet candidatesByCase;
    candidatesByCase.problemsByCase.resize(1); 
    
    int size = problems.size();
    //for(MDMKRawProblem problem : problems)
    for(int i{0}; i < size; i++)
    {
        
        separateCandidatesByCases(problems[i], candidatesByCase);
        cases.push_back(candidatesByCase);
        cases[i].knapsackCapacityVals = problems[i].getknapsackCapacityVals();
        cases[i].knapsackDemandRequirementVals = problems[i].getknapsackDemandRequirementVals();
        candidatesByCase.problemsByCase.clear();
    }
}


void readAttributeOfMDMKP(ifstream& file, vector<vector<long>>& candidateCoefficientAtrributes, vector<long>& knapsackGoalVals, long candidateCount, long leConstraints, bool isConstraint)
{
    long placeholder; 
    //leConstraints means <= constraints (also known as capacity constraints)
    for(int m{0}; m < leConstraints; m++) //reading for all candidate constraints
    {
        candidateCoefficientAtrributes.push_back(vector<long>());
        for(int i{0}; i < candidateCount; i++) // reading all <
        {
            file >> placeholder;
            candidateCoefficientAtrributes[m].push_back(placeholder);
        }
    }
    if(isConstraint)
        for(int m{0}; m < leConstraints; m++) //Reads capacity of knapsack for each capacity constraint
        {
            file >> placeholder;
            knapsackGoalVals.push_back(placeholder);
        }
}


//Param of fileName is the file we want to read from, must be in the same folder as this file (though I could easily change this if needed)
void readMDMKP(fs::path filePath, vector<MDMKRawProblem>& MDMKRawProblems) // read MDMKP problem text files in accordance to the format done by https://people.brunel.ac.uk/~mastjjb/jeb/orlib/mdmkpinfo.html
{
    
    ifstream file{filePath};
    long testProblemCount = 1, candidateCount, leConstraints; //leConstraints means <= constraints (also known as capacity constraints)
    
    /* file >> testProblemCount; */

    for(int i{0}; i < testProblemCount; i++) //traverses all the problem sets of the file 
    {
        string throwaway; //ignores demand constraint, as we infer demand/capacity are equal in count
        MDMKRawProblem problemSet; //creates a new empty problem set for every instance
        file >> candidateCount >> leConstraints >> throwaway; // These are the "header" variables for the brunel samples, they apply to all cases of a single problem (6 cases)

        readAttributeOfMDMKP(file, problemSet.getCandidateValue(), problemSet.getknapsackCapacityVals(), candidateCount, 1, false); // reads value of each object (Though the Brunel paper calls these "Cost" coefficients). 
        
        // vector<vector<long>> candidateCapacityAtrributes; //each vector contains a separate attribute for every candidate 
        vector<long> knapsackCapacityVals;
        readAttributeOfMDMKP(file, problemSet.getcandidateCapacityAtrributes(), problemSet.getknapsackCapacityVals(), candidateCount, leConstraints, true); // reads <= constraint

        vector<vector<long>> candidateDemandAtrributes;
        vector<long> knapsackDemandRequirementVals;
        readAttributeOfMDMKP(file, problemSet.getcandidateDemandAtrributes(), problemSet.getknapsackDemandRequirementVals(), candidateCount, leConstraints, true); // reads >= constraint

        // vector<vector<long>> candidateCostAtrributes; //Note that "knapsackCapacityVals" is useless here due to the boolean parameter being = false
        //I repurposed the leconstraints parameter to be the # of cases each model has, which is 6 for Brunel test cases
        
        MDMKRawProblems.push_back(problemSet);
    }
}

///////////////////////////////////
int extractNumber(const std::string& path, const std::string& pattern) {  //This function is NOT MADE BY ME
    std::regex re(pattern);
    std::smatch match;
    if (std::regex_search(path, match, re) && match.size() > 1) {
        return std::stoi(match[1]);
    }
    return 0;
}

vector<vector<int>> getWarmSols(string directoryName, int candidateCount)
{
    string read;
    vector<vector<int>> ans;
    vector<filesystem::directory_entry> unsortedDirs; // We need to define sorted order (problem 1 -> 15)
    for(const auto& folderInDir : filesystem::recursive_directory_iterator(directoryName))
        if(folderInDir.is_directory() == true)
            unsortedDirs.push_back(folderInDir);
    
       
/*     std::sort(sortedDirs.begin(), sortedDirs.end(), /// NOT MADE BY ME
        [](const filesystem::path& a, const filesystem::path& b) {
            int aB = extractNumber(a.string(), R"(B(\d+)C)");
            int aC = extractNumber(a.string(), R"(B\d+C(\d+))");
            int bB = extractNumber(b.string(), R"(B(\d+)C)");
            int bC = extractNumber(b.string(), R"(B\d+C(\d+))");
            
            if (aB != bB) return aB < bB;
            return aC < bC;
        });
 */
    for(const auto dir : unsortedDirs)
    {
        for(const auto& itemInDir : filesystem::recursive_directory_iterator(dir))
        {
            if(itemInDir.is_regular_file() == true && itemInDir.path().stem() == "run_000_best_solution") // sol must only be name this
            {
                vector<int> sol;
                
                getline(ifstream{itemInDir.path()}, read);
                istringstream stringifiedDecisions{read};
                for(int i{0}; i < candidateCount; i++)
                {
                    try
                    {
                        char commaConsume;
                        int decision;
                        stringifiedDecisions >> decision >> commaConsume; //commaConsume is only for consumption, not usage
                        sol.push_back(decision);
                    } catch(exception& e)
                    {
                        continue;
                    }
                    
                }
                ans.push_back(sol);            
            }
        }
    }
    return ans;
}


void runGurobiMDMKPWarm(GRBEnv& env, ofstream& excel, vector<problemSet>& caseNums, fs::path p, vector<int> sol)
{
    int blockNum{1};
    /////////////////////////////////////// Test branch code!!!! (ONCE AGAIN, REMOVE THIS AFTER DONE THE TESTING)
    for(auto caseNum : caseNums)
    {
      /*   for(long& demandRequirement : caseNum.knapsackDemandRequirementVals) //modifying the demand requirements (loosening requirements)
            demandRequirement *= 0.8; */
    /////////////////////////




        //code specific for removing cases that did get feasible solutions in previous tests so we can test the core problem approach only on hard problems
/*         if(blockNum != 14)
        {    
            blockNum++;
            continue;
        } */





        vector<GRBLinExpr> demandConstr;
        vector<GRBLinExpr> capacityConstr;
        GRBLinExpr objective;

        
        GRBModel model(env);
        
        
        
        model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program  ADD BACK ERIOJERIJGERJOGERJIOGERIOGREJIOGERJIOGERJIORJIOERGJIOERGERJIEGJI
        model.set(GRB_DoubleParam_TimeLimit, 0.1); 
        //model.set(GRB_DoubleParam_TimeLimit, 3600); 

        vector<GRBVar> x; //variable for if we include / not include item in knapsack
//////////////////// objective value definition ///////////////
        for(int i{0}; i < caseNum.problemsByCase[0].size(); i++) 
            x.push_back(model.addVar(0.0, 1.0, 0.0, GRB_BINARY)); 
       
        for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
                objective += caseNum.problemsByCase[0][i].value * x[i]; 
        model.setObjective(objective, GRB_MAXIMIZE);
//////////////////// capacity constraint ///////////////
        for(int i{0}; i < caseNum.knapsackCapacityVals.size(); i++) 
        {
            GRBLinExpr capacityExpr;
           
            for(int e{0}; e < caseNum.problemsByCase[0].size(); e++)
                capacityExpr += caseNum.problemsByCase[0][e].capacityVal[i] * x[e]; // REMINDER: FOR NON CASE 1, edit demandVAL[0]
                
            capacityConstr.push_back(capacityExpr);
        }
        for(int i{0}; i < capacityConstr.size(); i++)
            model.addConstr(capacityConstr[i] <= caseNum.knapsackCapacityVals[i] );
       
//////////////////// demand constraint ///////////////
        for(int i{0}; i < caseNum.problemsByCase[0][0].demandVal.size(); i++)  // NEED TO CHECK CASE VAL REIJEIOGJIGJERJIGOERGIOREGJIERGJIOERGJIOERGJIOGJOERGJO
        {
            GRBLinExpr demandExpr;
            for(int e{0}; e < caseNum.problemsByCase[0].size(); e++)
            {
                    //demandExpr += case1.problemsByCase[0][e].demandVal[dCount] * x[i]; // REMINDER: FOR NON CASE 1, edit demandVAL[0]
                    demandExpr += caseNum.problemsByCase[0][e].demandVal[i] * x[e]; // REMINDER: FOR NON CASE 1, edit demandVAL[0]
            }
            demandConstr.push_back(demandExpr);
        }
        for(int i{0}; i < demandConstr.size(); i++)
            model.addConstr(demandConstr[i] >= caseNum.knapsackDemandRequirementVals[i] );
      
        


      /*   // ///////////////////// EXPERIMENT DELETE ERIGGIRJREGJGJIEIJ   preSparsify 1
        model.set(GRB_DoubleParam_TuneTimeLimit,600);
        model.tune();
        int resultcount = model.get(GRB_IntAttr_TuneResultCount);
        if(resultcount > 0)
        {
            model.getTuneResult(resultcount-1);
            model.write("tune.prm");
        }
        
        exit(EXIT_SUCCESS);



        
        //////////////////////////// */
        /// Warm start code 
        for(int i{0}; i < sol.size(); i++)
            x[i].set(GRB_DoubleAttr_Start, sol[i]);

        // model.write("testModel.lp"); //Insane new method I learned that helps a lot with debugging, outputs a file that visually shows what the model holds
        model.optimize();

        long long profit{0};

        
        
        
        if(model.get(GRB_IntAttr_SolCount) > 0)
        {
            for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
            {
                if( x[i].get(GRB_DoubleAttr_X) >= 0.5) //Turns out x can only be a double, so we must use a bound rather than an exact value
                    profit += caseNum.problemsByCase[0][i].value;
            }        
            excel << p.filename() << "," <<  profit << "," << model.get(GRB_DoubleAttr_Runtime) << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
          /*  for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
            {
                excel << i << ',';
            }  
            excel << endl;
            for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
            {
                excel << x[i].get(GRB_DoubleAttr_X) << ',';
            }    
            excel << endl; */
        }
        else // case of infeasible solution 
        {
            profit = -1;
            excel << p.filename() << "," <<  profit << "," << model.get(GRB_DoubleAttr_Runtime) << endl; 
        }
        blockNum++;



    }
}


//Stores all the problems for 1 of the 6 cases in the caseSet vector
void formatCase(int caseNum, vector<problemSet>& caseSet, vector<problemSet>& problemSets) //caseSet should be empty! Holds the answer
{
    for(int i{0}; i < problemSets.size(); i++)
    {
        problemSet caseProblem;
        caseProblem.problemsByCase.push_back(problemSets[i].problemsByCase[caseNum]);
        caseProblem.knapsackCapacityVals = problemSets[i].knapsackCapacityVals;
        caseProblem.knapsackDemandRequirementVals = problemSets[i].knapsackDemandRequirementVals;
        caseSet.push_back(caseProblem);
    }
}


int main()
{
    ofstream excel("SamehTestCases.csv"); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
    excel << "Name" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << '\n';

    GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    (env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    (env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    (env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
    env.start();


    fs::path p = ("C:/Users/Pomer/Desktop/Gurobi projects/MDMKP/Sameh_n1000Dataset");
    int blockNum{0};
    vector<vector<int>> warmSols = getWarmSols("C:/Users/Pomer/Desktop/Gurobi projects/Garcia_shihabi_results_600s", 1000);
    
    for(const fs::directory_entry file : fs::recursive_directory_iterator(p))
    {
        /*  if(blockNum < 45) //// use this code to start at a point other than block 1
        {
            blockNum++;
            continue;
        }  */
        //reading 
        vector<MDMKRawProblem> MDMKRawProblem;
        readMDMKP(file.path(), MDMKRawProblem);
        //readMDMKP("mdmkp_ct8.txt", MDMKRawProblems);
        vector<vector<MDMKCandidate>> candidatesByCase;
        vector<problemSet> problemSets;
        RawProblemsToCases(MDMKRawProblem, problemSets);

    /*     for(int i{0}; i <= 5; i++) //extracts cases 1-6 and runs gurobi on them
        {
            vector<problemSet> caseSet;
            formatCase(i, caseSet, problemSets);
            runGurobiMDMKP(env, excel, caseSet, i);
        } */

        vector<problemSet> caseSet;
        formatCase(0, caseSet, problemSets);
        
        runGurobiMDMKPWarm(env, excel, caseSet, file.path(), warmSols[blockNum++]);
        
    }
    return 0;
}
