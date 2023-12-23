#include<iostream>
#include <bits/stdc++.h>

using namespace std;

class Solution {
public:
    vector<bool> kidsWithCandies(vector<int>& candies, int extraCandies) 
    {

        vector<bool> output;
        
        for (auto it = candies.begin(); it != candies.end(); it++){
            int kid1Candy = *it + extraCandies;
            cout<<"kid1Candy :"<<kid1Candy<<endl;
            auto it1 = candies.begin();
            for (; it1 != candies.end(); it1++) {
                cout<<"it1 is :"<<*it1<<endl;
                if(kid1Candy < *it1) {
                    break;
                }
            }
            if(it1 == candies.end()) {
                output.push_back(true);
            }
            else {
                output.push_back(false);
            }
            for(auto element:output){
            cout<<element<<endl;
            }
        }
        return output;
    }
};


int main() {
    vector <int> candies = {2,3,5,1,3};
    vector<bool> output;
    Solution obj1;
    
    output = obj1.kidsWithCandies(candies, 3);
    for(auto element:output) {
        cout<<element<<'\t';
    }
}