#include "ConfigValidator.hpp"
#include "Utils.hpp"
#include <cstdlib>

ConfigValidator::ConfigValidator() { initRules(); }
ConfigValidator::~ConfigValidator() { }

void ConfigValidator::initRules() {
    _rules["server"] = (DirectiveRule){0, 0, (1 << CTX_MAIN), false, true};
    _rules["location"] = (DirectiveRule){1, 1, (1 << CTX_SERVER) | (1 << CTX_LOCATION), false, true};
    _rules["limit_except"] = (DirectiveRule){1, 3, (1 << CTX_LOCATION), true, true};
    _rules["listen"] = (DirectiveRule){1, 1, (1 << CTX_SERVER), false, false};
    _rules["server_name"] = (DirectiveRule){1, 10, (1 << CTX_SERVER), true, false};
    _rules["root"] = (DirectiveRule){1, 1, (1 << CTX_SERVER) | (1 << CTX_LOCATION), true, false};
    _rules["index"] = (DirectiveRule){1, 99, (1 << CTX_SERVER) | (1 << CTX_LOCATION), false, false};
    _rules["error_page"] = (DirectiveRule){2, 99, (1 << CTX_SERVER) | (1 << CTX_LOCATION), false, false};
    _rules["autoindex"] = (DirectiveRule){1, 1, (1 << CTX_SERVER) | (1 << CTX_LOCATION), true, false};
    _rules["client_max_body_size"] = (DirectiveRule){1, 1, (1 << CTX_MAIN) | (1 << CTX_SERVER) | (1 << CTX_LOCATION), true, false};
    _rules["return"] = (DirectiveRule){2, 2, (1 << CTX_SERVER) | (1 << CTX_LOCATION), true, false};
    _rules["cgi_pass"] = (DirectiveRule){1, 2, (1 << CTX_LOCATION), true, false};
    _rules["upload_store"] = (DirectiveRule){1, 1, (1 << CTX_LOCATION), true, false};
    _rules["deny"] = (DirectiveRule){1, 1, (1 << CTX_LIMIT_EXCEPT), false, false};
}

void ConfigValidator::validate(ConfigNode* root) {
    if (!root) 
        return;
    validateNode(root, CTX_MAIN);
}

void ConfigValidator::validateNode(ConfigNode* node, int current_context) {
    if (node->name == "ROOT") {
        for (size_t i = 0; i < node->children.size(); i++) {
            validateNode(node->children[i], current_context);
        }
        return;
    }

    if (_rules.find(node->name) == _rules.end()) {
        throw std::runtime_error("Validation error: Unknown directive '" + node->name + "'");
    }

    DirectiveRule rule = _rules[node->name];

    if (!((1 << current_context) & rule.allowed_contexts)) {
        throw std::runtime_error("Validation error: Directive '" + node->name + "' is not allowed in the current context");
    }

    if (node->args.size() < (size_t)rule.min_args || node->args.size() > (size_t)rule.max_args) {
        throw std::runtime_error("Validation error: the args number is not the allowed one in the Directive '" + node->name + "'");
    }

    if (rule.is_unique && node->parent) {
        int count = 0;
        for (size_t i = 0; i < node->parent->children.size(); i++) {
            if (node->parent->children[i]->name == node->name) {
                count++;
            }
        }
        if (count > 1) {
            throw std::runtime_error("Validation error: Duplicate directive '" + node->name + "'");
        }
    }

    if (rule.is_block && node->type != NODE_BLOCK) {
        throw std::runtime_error("Validation error: Directive '" + node->name + "' must be a block (ending with '{')");
    }
    if (!rule.is_block && node->type == NODE_BLOCK) {
        throw std::runtime_error("Validation error: Directive '" + node->name + "' cannot be a block (should end with ';')");
    }

    int next_context = current_context;
    if (node->name == "server") next_context = CTX_SERVER;
    if (node->name == "location") next_context = CTX_LOCATION;
    if (node->name == "limit_except") next_context = CTX_LIMIT_EXCEPT;

    for (size_t i = 0; i < node->children.size(); i++) {
        validateNode(node->children[i], next_context);
    }

    if(node->name == "listen") parseListenValue(node->args[0]);
    if(node->name == "error_page") {
        for(size_t i = 0; i < node->args.size(); i++)
            if(i != node->args.size() - 1)
                parse_http_code(node->args[i]);
    }
    if(node->name == "client_max_body_size")
        parseCbmz(node->args[0]);
    if(node->name == "autoindex") {
        if(node->args[0] != "on" && node->args[0] != "off")
            throw std::runtime_error("Error: invalid value for autoindex (expected 'on' or 'off')");
    }
    if(node->name == "return")
        parse_http_code(node->args[0]);
}
