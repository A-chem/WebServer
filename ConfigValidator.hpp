#ifndef CONFIG_VALIDATOR_HPP
#define CONFIG_VALIDATOR_HPP

#include "ConfigNode.hpp"
#include <map>
#include <string>
#include <stdexcept>

#define CTX_MAIN         0
#define CTX_SERVER       1
#define CTX_LOCATION     2
#define CTX_LIMIT_EXCEPT 3

struct DirectiveRule {
    int     min_args;
    int     max_args;
    int     allowed_contexts; 
    bool    is_unique;        
    bool    is_block;         
};

class ConfigValidator {
private:
    std::map<std::string, DirectiveRule> _rules;

    void initRules();
    void validateNode(ConfigNode* node, int current_context);

public:
    ConfigValidator();
    ~ConfigValidator();

    void validate(ConfigNode* root);
};

#endif
