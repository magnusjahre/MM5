
#include "InterconnectTester.hh"
        
using namespace std;
        
START_TEST (butterfly_radix_2)
{
    Butterfly* butterfly;
    
    butterfly = new Butterfly("test_butterfly", 64, 1, 1, 1, 8, NULL, 1, 2, 4);
    
    butterfly->request(0, 0);
    butterfly->request(0, 1);
    butterfly->request(0, 2);
    butterfly->request(0, 3);
    butterfly->request(0, 4);
    
    butterfly->arbitrate(1);
    
}
END_TEST
        
        
int main(int argc, int argv){
    int number_failed;
    
    Suite *s = butterfly_suite();
    SRunner *sr = srunner_create(s);
    
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

Suite* butterfly_suite(){
    Suite* s = suite_create("Butterfly");
    
    TCase *tc_butterfly = tcase_create("8 Core");
    tcase_add_test(tc_butterfly, butterfly_radix_2);
    suite_add_tcase(s, tc_butterfly);
            
    
    return s;
}

