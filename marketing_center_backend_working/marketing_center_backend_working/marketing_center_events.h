#define DISPENSE_CARD_EVENT "dispense_card"
#define DISPENSE_CARD_FMT "4s1 card"
typedef struct {
	int32_t card;
} dispense_card_event_t;

#define DISPENSER_STATUS_EVENT "dispenser_status"
#define DISPENSER_STATUS_FMT "1s1 status"
typedef struct {
	int8_t status;
} dispenser_status_event_t;

#define CHANGE_SCREEN_1_EVENT "change_screen_1"

#define DISPENSER_IS_EMPTY_EVENT "dispenser_is_empty"
#define DISPENSER_IS_EMPTY_FMT "1s1 emptyOrNot"
typedef struct {
	int8_t emptyOrNot;
} dispenser_is_empty_event_t;

#define PRINT_RECEIPT_EVENT "print_receipt"
#define PRINT_RECEIPT_FMT "4s1 receipt"
typedef struct {
	int32_t receipt;
} print_receipt_event_t;


#define LINK_TEST_EVENT "link_test"
#define LINK_TEST_FMT "1s1 l"
typedef struct {
	int8_t l;
} link_test_event_t;


#define LINK_TEST_WORKING_EVENT "link_test_working"
#define LINK_TEST_WORKING_FMT "4s1 link_data 1s1 link"
typedef struct {
	int32_t link_data;
	int8_t link;
} link_test_working_event_t;

