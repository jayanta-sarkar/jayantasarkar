// To find gcd among 2 given strings
#include <iostream>
using namespace std;

class gcd {
    
    public:
string findGcd(string s1, string s2) {
    
    if (s1.length() < s2.length() ) {
        return findGcd (s2, s1);
    }
    else if(s1.find(s2)!=0) {
        return "";
    }
    else if(s2 == "") {
        return s1;
    }
    else {
        
        return findGcd(s1.substr(s2.length()), s2);
    }
    }
};

int main() {
  
    std::cout << "Hello world! \n";
    
    string str = "ABABAB";
  string str1 = "ABAB";
    
    gcd obj1;
    
    cout<<obj1.findGcd(str, str1);

    return 0;
}