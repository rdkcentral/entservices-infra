#include <string>
#include <jq.h>
#include <jv.h>

//#include "UtilsLogging.h"
using namespace std;

class JQWrapper {

    public:
    JQWrapper() {
        jq = jq_init();
        if (!jq) {
            printf("Failed to initialize jq");
        }
    }

    ~JQWrapper() {
        if (jq) {
            jq_teardown(&jq);
        }
    }

    string compile(string jq_rule, string json) {
        if (!jq_compile(jq, jq_rule.c_str())) {
            printf("Failed to compile jq");
        }

        jv input = jv_parse(json.c_str());
        if (jv_is_valid(input)) {
            jq_start(jq, input, 0);

            jv result;
            while (jv_is_valid(result = jq_next(jq))) {
                jv  val = jv_dump_string(result, 0);
                const char *str = jv_string_value(val);
                return str;
            }
        }
    }

    void test() {
        string jq_rule = "{ first }";
        string json = R"({"first": "Suresh", "last": "Kumar"})";

        string result = compile(jq_rule, json);
        printf("jq output: %s\n", result.c_str());
        /*
            [186138] DEBUG [HttpClient.cpp:31] HttpClient: curl initialized
            jq output: "Suresh"
            [186138] INFO [PackageManagerImplementation.cpp:122] Initialize: entry
        */
    }
    private:
        jq_state* jq = nullptr;

};
