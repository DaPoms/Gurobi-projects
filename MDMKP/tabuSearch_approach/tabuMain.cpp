#include "gurobi_c++.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
using namespace std;
//New includes (unique to this iteration)
#include <random> // for randomizing shuffle()
#include <algorithm> // for shuffle()
#include <unordered_map>
#include <chrono> // for using the clock to select a random seed



//Note: test picking best sol, capacity only, and demand only for the tabu search input


///////////////////// start of file reading code /////////////////////

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
    candidatesByCase.problemsByCase.resize(6); // new thing learned! Can be used to easily create uninitialized values inside of the vector (or reshape vector to fit this function's usage)
    vector<vector<long>> MDMKCapacityAttributes = problem.getcandidateCapacityAtrributes(); 
    vector<vector<long>> MDMKDemandAttributes = problem.getcandidateDemandAtrributes();
    vector<vector<long>> MDMKValue = problem.getCandidateValue();
    long capacityVarsCount = MDMKCapacityAttributes.size(); //Shortcut to finding the amount of capacity variables (dimensions) for the problem
    long candidateCount = MDMKCapacityAttributes[0].size();//This is a way to find the # of candidates we have without having to pass directly (not too efficient, but less params = simpler). Only works if problem is not empty
    int caseDemand1 = 1; //sort of pointless but slightly more readable as it describes what the value 1 is for (Case 1 of MDMKP problems)
    int caseDemand2 = capacityVarsCount / 2;
    int caseDemand3 = capacityVarsCount; // Note that case 1,2,3 are repeated for case 4,5,6 respectively
     int order[6] = {0,3,1,4,2,5}; //order which we want to get candidates (most efficient order for the cases)
    for(int i{0}; i < candidateCount; i++)
    {
        MDMKCandidate candidate;
        for(int c{0}; c < capacityVarsCount; c++)
            candidate.capacityVal.push_back(MDMKCapacityAttributes[c][i]);
        
       
        for(int caseNum{0}; caseNum < 6; caseNum++) //Changes candidate to each case (note that caseNum = 0 is case 1 and caseNum = 5 is case 6)
        {
            int currentCase = order[caseNum];
            candidate.value = MDMKValue[currentCase][i];

            for(int currDemand{0}; currDemand < capacityVarsCount; currDemand++) //stops at case 6 (where same # of demands as capacity constraints)
            {
                // WARNING: Very ugly if statement below!
                candidate.demandVal.push_back(MDMKDemandAttributes[currDemand][i]);

                if ( (currDemand == caseDemand3 - 1 && (currentCase == 2 || currentCase == 5)) || (currDemand == caseDemand2 - 1 && (currentCase == 1 || currentCase == 4) ) || (currDemand == caseDemand1 - 1 && (currentCase == 0 || currentCase == 3)) ) //these checks are really inefficient but they work!
                {
                    candidatesByCase.problemsByCase[currentCase].push_back(candidate);
                    
                }      
            }
            
            candidate.demandVal.clear();///
        }
        
    }
}

void RawProblemsToCases(vector<MDMKRawProblem>& problems, vector<problemSet>& cases)
{
    problemSet candidatesByCase;
    candidatesByCase.problemsByCase.resize(6);
    //cases.resize(0);
    /* for(problemSet problem : cases)
        problem.problemsByCase.resize(6); */
    
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
void readMDMKP(string fileName, vector<MDMKRawProblem>& MDMKRawProblems) // read MDMKP problem text files in accordance to the format done by https://people.brunel.ac.uk/~mastjjb/jeb/orlib/mdmkpinfo.html
{
    
    ifstream file{fileName};
    long testProblemCount, candidateCount, leConstraints; //leConstraints means <= constraints (also known as capacity constraints)
    
    file >> testProblemCount;

    for(int i{0}; i < testProblemCount; i++) //traverses all the problem sets of the file 
    {
        MDMKRawProblem problemSet; //creates a new empty problem set for every instance
        file >> candidateCount >> leConstraints; // These are the "header" variables for the brunel samples, they apply to all cases of a single problem (6 cases)
        vector<vector<long>> candidateCapacityAtrributes; //each vector contains a separate attribute for every candidate 
        vector<long> knapsackCapacityVals;
        readAttributeOfMDMKP(file, problemSet.getcandidateCapacityAtrributes(), problemSet.getknapsackCapacityVals(), candidateCount, leConstraints, true); // reads <= constraint

        vector<vector<long>> candidateDemandAtrributes;
        vector<long> knapsackDemandRequirementVals;
        readAttributeOfMDMKP(file, problemSet.getcandidateDemandAtrributes(), problemSet.getknapsackDemandRequirementVals(), candidateCount, leConstraints, true); // reads >= constraint

        vector<vector<long>> candidateCostAtrributes; //Note that "knapsackCapacityVals" is useless here due to the boolean parameter being = false
        //I repurposed the leconstraints parameter to be the # of cases each model has, which is 6 for Brunel test cases
        readAttributeOfMDMKP(file, problemSet.getCandidateValue(), problemSet.getknapsackCapacityVals(), candidateCount, 6, false); // reads value of each object (Though the Brunel paper calls these "Cost" coefficients). 
        
        MDMKRawProblems.push_back(problemSet);
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

///////////////////// end of file reading / data organizing code /////////////////////

template <typename T = int> // Just decided to refresh myself on templating, it's been a while
void generatePermutation(vector<T>& toBeShuffled)
{
    unsigned int seed = chrono::system_clock::now().time_since_epoch().count(); //keeps track of time without needing to update manually, which holds a benefit over std::time_t
    mt19937 generator(seed); //change the seed to ensure random permutations
    shuffle(toBeShuffled.begin(), toBeShuffled.end(), generator);
}

//it's assumed that the problemSet only contrains one vector in its .problemByCase 
bool isAddAllowedCapacity(vector<long>& knapsackCurrCapacity, problemSet& problem, int targetI) //returns if item can be added without violating <= 
{
    for(int c{0}; c < problem.knapsackCapacityVals.size(); c++) 
    {
        if(knapsackCurrCapacity[c] + problem.problemsByCase[0][targetI].capacityVal[c] > problem.knapsackCapacityVals[c]) 
            return false;
    }
    return true;
}

bool isRemoveAllowedDemand(vector<long>& knapsackCurrDemandTotals, problemSet& problem, int targetI) //returns if item can be removed without violating >= 
{
    for(int d{0}; d < problem.knapsackDemandRequirementVals.size(); d++) 
    {
        if(knapsackCurrDemandTotals[d] - problem.problemsByCase[0][targetI].demandVal[d] < problem.knapsackDemandRequirementVals[d]) 
            return false;
    }
    return true;
}


// to work, currDemand/CapacityVals must be the same size as the respective problemSets demand/capacity limits
bool isFeasible(problemSet& problem, vector<long>& currCapacityVals, vector<long> currDemandVals)
{
    for(int c{0}; c < currCapacityVals.size(); c++) // curr must be <= the problem's cap to be feasible
        if(currCapacityVals[c] > problem.knapsackCapacityVals[c]) return false;

    for(int d{0}; d < currDemandVals.size(); d++) // curr must be >= the problem's cap to be feasible
        if(currDemandVals[d] < problem.knapsackCapacityVals[d]) return false;

    return true;
}

void sumConstraints(problemSet& problem, vector<bool>& decisionVars, vector<long>& capacityAns, vector<long>& demandAns) // capacity/demandAns are just parameters that hold the answer (the sum of each constraint, demand/capacity)
{
    vector<long> currCapacityVals(problem.knapsackCapacityVals.size(), 0);
    vector<long> currDemandVals(problem.problemsByCase[0][0].demandVal.size(), 0);
    for(int i{0}; i < decisionVars.size(); i++) //goes through all candidates. This would also work with i < problem.problemByCase[0].size() but that is less readable to me. 
    {
        if(decisionVars[i] == true)
        {
            for(int c{0}; c < problem.knapsackCapacityVals.size(); c++) // curr must be <= the problem's cap to be feasible
                currCapacityVals[c] += problem.problemsByCase[0][i].capacityVal[c];
            for(int d{0}; d < problem.problemsByCase[0][0].demandVal.size(); d++) // Must use the demand vector stored in a candidate as the candidate demands shows how many demands are being considered for the case.
                                                                                  //curr must be >= the problem's cap to be feasible
                currDemandVals[d] += problem.problemsByCase[0][i].demandVal[d];
        }
    }
    //stores answer in the 2 passed vector<long>'s
    capacityAns = currCapacityVals;
    demandAns = currDemandVals;
}

//just sumOffBy but it considers both demand and capacity for offby, not just one. (In cases where we arent solving just one side, such as tabu-search)
long sumOffByTotal(problemSet& problem, vector<long>& currCapacityTotals, vector<long>& currDemandTotals) //returns the absolute value sum of how off the knapsack was from a feasible solution
{
    vector<long>& knapsackCapacityLimits = problem.knapsackCapacityVals;
    vector<long>& knapsackDemandLimits = problem.knapsackDemandRequirementVals;
    long offBy{0};

        for(int i{0}; i < knapsackCapacityLimits.size(); i++) 
            if(knapsackCapacityLimits[i] < currCapacityTotals[i]) //only difference caused by isSolvedForCapacity
                offBy += (knapsackCapacityLimits[i] - currCapacityTotals[i]) * -1; //we do abs val to make sure being off hurts the solutions score for demands (higher offBy val == worse solution)

        for(int i{0}; i < problem.problemsByCase[0][0].demandVal.size(); i++) // once again have to use the size of demand vector of a candidate as only the candidate shows how many demands are considered for a given case/problem
            if(knapsackDemandLimits[i] > currDemandTotals[i]) 
                offBy += (knapsackDemandLimits[i] - currDemandTotals[i] ); // offBy uses the absolute value, and this if statement guarantees this to be negative , so we * -1 to make it positive
        
    return offBy;
}

long sumOffBy(vector<long>& knapsackConstraintLimits, vector<long> currConstraintTotals, bool isSolvedForCapacity) //returns the absolute value sum of how off the knapsack was from a feasible solution for a solution that is feasible for one side (demand OR capacity constraints)
{ // I do admit using isSolvedForCapacity is very clunky and makes the code less readable/more redundant
    long offBy{0};
    if(isSolvedForCapacity) // if isSolvedForCapacity is true, it means that the offBy for capacity for each constraint will = 0, so we ignore it in our offBy calculation, only caring for demands
    {
        for(int i{0}; i < currConstraintTotals.size(); i++) // must use currConstraintVals due to the fact that currConstraintVals when passed as the currDemand vector determines how many demands the problem has, whereas knapsackConstraintLimits is always the max case (case 3/6)
        {
            if(knapsackConstraintLimits[i] > currConstraintTotals[i]) //only difference caused by isSolvedForCapacity
                offBy += knapsackConstraintLimits[i] - currConstraintTotals[i];
        }
    }
    else //else is demand. Same reasoning as for capacity, if the problem was solved to satisfy all demands, than offBy for the demands = 0, so we only care about capacity vals
    {
        for(int i{0}; i < currConstraintTotals.size(); i++) 
        {
            if(knapsackConstraintLimits[i] < currConstraintTotals[i]) 
                offBy += (knapsackConstraintLimits[i] - currConstraintTotals[i] ) * -1; // offBy uses the absolute value, and this if statement guarantees this to be negative , so we * -1 to make it positive
        }
    }
    return offBy;
}

// knapsackConstraintLimit are the requirements the knapsack problem asks for, either of demands or capacity constraints (EX: demand 1 must be >= 300, capacity 0 must be <= 124)
bool isBestSol(vector<long>& knapsackConstraintLimit, vector<long>& currConstraintTotals, long currBestTotalOffBy, long& candidateOffBy, bool isCandidateSolvedForCapacity) //best sol is the one closest to feasibility, though if both are feasible (the current best and the one to be picked, its picks the best obj val total between them)
{ //the bool isCandidateSolvedForCapacity reduces redundancy in processing (via sumOffBy()) but does force my code to be less readable
    candidateOffBy = sumOffBy(knapsackConstraintLimit, currConstraintTotals, isCandidateSolvedForCapacity);
    if (candidateOffBy < currBestTotalOffBy) 
        return true; // for the best, the lower the value, the better
    return false;
}

vector<bool> mapSolutionToVector(unordered_map<int,bool>& knapsackSolution) // I got lazy here, I can add templating later, but this is very easy to impliment anyways
{
    vector<bool> solution(knapsackSolution.size());
    for(int i{0}; i < knapsackSolution.size(); i++)
        solution[i] = knapsackSolution[i];
    return solution;
}


//TO DO: Add returning the BEST solution at the end, regardless if its feasible or not, as tabu search will use it 
vector< vector<bool> > arbitraryPermutationSolver(problemSet& problem) //main running variant
{
    vector< vector<bool> > answer(1); // whatever we deem to be best will be stored here (will try closest to feasible, capacity only, and demand only approaches)
    long bestTotalOffBy{INT_MAX}; // This will hold the absolute value sum of how much the best solution is off by for it to become feasible. The best solution is one closest to feasibility
    long candidateOffBy; // holds the offBy value of contenders for the bestTotalOffBy
    const int amountOfPermutations = 100; // just a variable to allow fast permutation count changing
    int candidateCount = problem.problemsByCase[0].size();
    vector<int> window(candidateCount); // I ultimately couldn't use the std::array library as for that library, arrays must have their size determined at compile time. For some reason my c-style arrays worked when they shouldn't, so I'm avoiding that practice
    for(int i{0}; i < candidateCount; i++) // the original window that will be scrambled from
        window[i] = i;
    unordered_map<int, bool> knapsackSolution; 

    // while(true) // this is an intentionally impossible to end for loop as we want this to only end when the solution is found (old approach used this, not we just pick the best answer)
    // {
        for(int i{0}; i < amountOfPermutations; i++) //generates amountOfPermutations count of the candidate order and tries to make them feasible (specified below)
        {
            vector<int> shuffled = window; //always shuffle from the starting array to make randomization completely independent. Note: copy constructor only works if you do not define size
            generatePermutation(shuffled);

            vector<long> currCapacityVals(problem.knapsackCapacityVals.size(), 0); //used for first phase
            vector<long> currDemandVals(problem.problemsByCase[0][0].demandVal.size(), 0); // used for second phase, but first must store the max possible demand values (if all candidates included)
            // Do note demandVals variate by case, so we use the amount of demands in a candidate to determine how many demand vals the problem has
            
            for(int i{0}; i < candidateCount; i++) // prepares solution for <= approach, in which on an empty knapsack you keep on adding to knapsack so long as the knapsack retains <= satisfaction
                    knapsackSolution[i] = 0;
            for(int i : shuffled) // <= approach. For every candidate...
            {
                if(isAddAllowedCapacity(currCapacityVals, problem, i)) //add item if it doesn't make a <= constraint false
                {
                    knapsackSolution[i] = 1;
                    for(int c{0}; c < currCapacityVals.size(); c++)
                        currCapacityVals[c] += problem.problemsByCase[0][i].capacityVal[c];
                    for(int d{0}; d < currDemandVals.size(); d++) //even though demand isn't directly needed, this allows very easy feasibility checking
                        currDemandVals[d] += problem.problemsByCase[0][i].demandVal[d];
                }
            }
            /* if(isFeasible(problem, currCapacityVals, currDemandVals)) //checks if problem is the solution
                return mapSolutionToVector(knapsackSolution); */
            if(isBestSol(problem.knapsackDemandRequirementVals, currDemandVals, bestTotalOffBy, candidateOffBy, true)) // note that currBestTotalOffBy just stores the answer of OffBy for the current solution, showing how off it is from being feasible
            {    
                bestTotalOffBy = candidateOffBy; 
                answer[0] = mapSolutionToVector(knapsackSolution); // for this implimentation, answer has only the best solution
            }

            //prepares solution for >= approach, in which on a knapsack holding every item you keep on removing from the knapsack so long as the knapsack retains >= satisfaction
            fill(currDemandVals.begin(), currDemandVals.end(), 0); //from algorithm library (could also be done with just a simple for loop). Resets the current demand vals for the next phase
            fill(currCapacityVals.begin(), currCapacityVals.end(), 0);
            for(int i{0}; i < candidateCount; i++) //stores knapsack demand + capacity vals if all items were put in the bag 
            {
                for(int d{0}; d < currDemandVals.size(); d++)
                    currDemandVals[d] += problem.problemsByCase[0][i].demandVal[d]; 
                for(int c{0}; c < currCapacityVals.size(); c++) // capacity is not necessary for this phase of the algorithm but rather an efficiency to speedup feasibility checking
                    currCapacityVals[c] += problem.problemsByCase[0][i].capacityVal[c];
            }
            for(int i{0}; i < candidateCount; i++) //For every candidate... 
                knapsackSolution[i] = 1; //we fill the bag with every single item, disregarding capacity
            for(int i : shuffled) // >= approach. For every candidate...
            {
                if(isRemoveAllowedDemand(currDemandVals, problem, i)) //remove item if it doesn't make a >= constraint false
                {
                    knapsackSolution[i] = 0;
                    for(int d{0}; d < currDemandVals.size(); d++)
                        currDemandVals[d] -= problem.problemsByCase[0][i].demandVal[d];
                    for(int c{0}; c < currCapacityVals.size(); c++)
                        currCapacityVals[c] -= problem.problemsByCase[0][i].capacityVal[c];
                }
            }
            /* if(isFeasible(problem, currCapacityVals, currDemandVals)) //checks if problem is the solution 
                return mapSolutionToVector(knapsackSolution); */
            if(isBestSol(problem.knapsackCapacityVals, currCapacityVals, bestTotalOffBy, candidateOffBy, false)) // we pass capacity vals as the opposite of what we solve for is considered in the best solution, as we already solved for the demand constraint to be met
            {    
                bestTotalOffBy = candidateOffBy; 
                answer[0] = mapSolutionToVector(knapsackSolution); 
            }
        }
    // }
    return answer; 
}


vector< vector<bool> > generateNeighborhood( vector<bool>& initSol) //generates the entire neighborhood of possible solutions. We do not pass by reference just so we dont have to make a useless extra var, and it makes more sense (a neighbor is just the initSol with candidate flipped (true -> false or false --> true) )
{
    int candidateCount = initSol.size();
    vector< vector<bool> > neighborhood(candidateCount); //when we know the size, this allows us to avoid using .push_back() which is less efficient than direct indexing assignment
    for(int i{0}; i < candidateCount; i++) // neighborhood is = the number of candidates there are, but each neighbor is exactly one candidate off from the original solution (ex: we change the first candidate from 1 -> 0)
    {
        initSol[i] = !initSol[i]; //flips i-th candidate
        neighborhood[i] = initSol;
        initSol[i] = !initSol[i]; //flips i-th candidate back to its original state (to prepare for the next neighbor without having to fully overwrite initSol)
    }

    return neighborhood;
}


//TODO: ASPIRATION CRITERION + TABU
vector<bool> tabuSearchMDMKP(vector<bool>& initSol, problemSet& problem)
{
    vector<bool> sol = initSol;
/*     if(isFeasible(problem, currCapacityTotals, currDemandTotals)) // DELETE. THIS IS DEBUGGING CODE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        cout << "1"; */
    //for/while statement goes here (the bulk of the behavior)
    // while() {
    
    vector<long> currCapacityTotals;
    vector<long> currDemandTotals;
    sumConstraints(problem, sol, currCapacityTotals, currDemandTotals); // calculates current demand/capacity totals (note we only have to do this once, as the ith neighbor will have a very similar demand/capacity Total but with the ith item taken out)
    long currOffBy = sumOffByTotal(problem, currCapacityTotals, currDemandTotals); //This is the offBy value of the solution that all the neighbors branched from
    int bestNeighborI{-1}; // the index of the best neighbor in the neighborhood which will be at the end used as the new root for forming the next neighborhood (thus this is related to currOffBy)

    long bestOffByAmongstNeighbors{INT_MAX}; // this is to store the backup solution, int_max guarantees the 1st neighbor will be stored in this var (as max is the worst possible answer)
    int bestNeighborAmongstNeighborsI; //index of the best solution between neighbors 
    vector< vector<bool> > neighborhood = generateNeighborhood(initSol);
    for(int i{0}; i < neighborhood.size(); i++) // for loop for picking the best neighbor
    {
        vector<long> neighborCapacityTotals;
        vector<long> neighborDemandTotals;
        sumConstraints(problem, neighborhood[i], neighborCapacityTotals, neighborDemandTotals);
        long neighborOffBy = sumOffByTotal(problem, neighborCapacityTotals, neighborDemandTotals);

        if(neighborOffBy < currOffBy) //normal case, picks the neighbor that improves most upon the root solution //NOTE TO SELF, NEED TO PROGRAM BEHAVIOR OF WHAT TO DO IF ALL TABU
        {
            currOffBy = neighborOffBy; // the neighbor is considered to be the next solution
            bestNeighborI = i;
        }
        else if(neighborOffBy < bestOffByAmongstNeighbors) //backup solution in case all neighbors are worse than the root solution (picks the best OffBy between the neighbors)
        { // Note that this case doesn't consider the root solution at all
            bestNeighborAmongstNeighborsI = i;
            bestOffByAmongstNeighbors = neighborOffBy; 
        }
    }
    // assigns the best neighbor compared to the root (if possible) or compared amongst neighbors as the root for the next neighborhood formation
    if(bestNeighborI != -1) 
        sol = neighborhood[bestNeighborI];
    else //if == -1, then it means no neighbor was better than the root solution, so we need to use our backup answer (the best answer amongst neighbors, not compared to the root)
        sol = neighborhood[bestNeighborAmongstNeighborsI];

    return vector<bool>();
}


/* template<typename T> // row will make a newline at the end
void outputVectorToCSVRow(ofstream& excel, vector<T>& passedVect, string optionalRowHeader = "")
{
    excel << optionalRowHeader << ","; //every element in a row needs to be separated by a comma for a csv ("comma separated value" file)
    for(int i{0}; i < passedVect.size(); i++)
    {
        excel << passedVect[i] << ",";
    }
    excel << '\n';
} */

template<typename t> //template is a function template, so we instead can pass a warm start function so long as it returns a vector of bools as the answer
void runWarmGurobiMDMKP(t warmStartFunction, GRBEnv& env, ofstream& excel,  vector<problemSet>& caseNums, int caseCounter)
{
    int blockNum{1};
    for(auto caseNum : caseNums)
    {
        vector<GRBLinExpr> demandConstr;
        vector<GRBLinExpr> capacityConstr;
        GRBLinExpr objective;
        GRBModel model(env);
        
        model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program 
        model.set(GRB_DoubleParam_TimeLimit, 1); //600 
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
                demandExpr += caseNum.problemsByCase[0][e].demandVal[i] * x[e]; // REMINDER: FOR NON CASE 1, edit demandVAL[0]
            
            demandConstr.push_back(demandExpr);
        }
        for(int i{0}; i < demandConstr.size(); i++)
            model.addConstr(demandConstr[i] >= caseNum.knapsackDemandRequirementVals[i] );
//////////////////// Warm start code ///////////////

auto start = chrono::high_resolution_clock::now();
vector< vector<bool> > initSols  = arbitraryPermutationSolver(caseNum); //initSol holds all the solutions that are to be considered one at a time by the tabu search algorithm (currently just one, the best solution)
auto stopTime = chrono::high_resolution_clock::now();
long long runTime = chrono::duration_cast<chrono::seconds>(stopTime - start).count();


vector<bool> warmSol = warmStartFunction(initSols[0], caseNum); // currently this is tabu search, uses the passed heuristic to get hopefully a feasible solution

//feeds solution as a warm start for gurobi
if(warmSol.size() > 0) // sort of pointless error catching but just in case
{
    for(int i{0}; i < warmSol.size(); i++)
        x[i].set(GRB_DoubleAttr_Start, warmSol[i]);
} 




///////////////////////////////////
        model.optimize();

        long long profit{0};

        //model.write("testModel.lp"); //Insane new method I learned that helps a lot with debugging, outputs a file that visually shows what the model holds
        
         
        if(model.get(GRB_IntAttr_SolCount) > 0)
        {
            for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
            {
                 if( x[i].get(GRB_DoubleAttr_X) >= 0.5) //Turns out x can only be a double, so we must use a bound rather than an exact value
                    profit += caseNum.problemsByCase[0][i].value; 
            }        
           

            excel << "B" << blockNum << "C" << (caseCounter + 1) << "," <<  profit << "," << runTime << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
        }   
       
        blockNum++; 
    }
}




int main()
{
   
    ofstream excel("MDMKP_tabu.csv"); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
   
    excel << "Name" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << '\n';
    //excel << "Solution Capacity Totals" << "," << "Capacity Right Coefficients (required val)" <<  "," << "Solution Demand totals" << "," << "Demand Right Coefficients" << '\n';

    GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    (env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    (env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    (env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
    env.start();

    //reading 
    vector<MDMKRawProblem> MDMKRawProblems;
    readMDMKP("datac7.txt", MDMKRawProblems);
    vector<vector<MDMKCandidate>> candidatesByCase;
    vector<problemSet> problemSets;
    RawProblemsToCases(MDMKRawProblems, problemSets);
  

 /*    for(int i{0}; i <= 5; i++) //extracts cases 1-6 and runs gurobi on them
    {
        vector<problemSet> caseSet;
        formatCase(i, caseSet, problemSets);
        runWarmGurobiMDMKP(tabuSearchMDMKP, env, excel, caseSet, i);
    } */

 
    vector<problemSet> case3Set; // case 3 
    formatCase(2, case3Set, problemSets); //yes an input of 2 means case 3
    runWarmGurobiMDMKP(tabuSearchMDMKP, env, excel, case3Set, 2); //you pass functions just by name
   

    /* 
    //case 6
    vector<problemSet> case6Set; // case 6
    formatCase(5, case6Set, problemSets);
    runGurobiMDMKP(env, excel, case6Set, 5);
    */

    
    return 0;
}
