#include "flash_lib.h"


#include "fstorage.h"


#include "nrf_drv_common.h"			// Needed for data in RAM check

#include "system_event_lib.h"	// Needed to register an system event handler!
#include "systick_lib.h"		// Needed for the timeout-checks

#include "debug_lib.h"


#define FLASH_OPERATION_TIMEOUT_MS		1000	/**< The time in milliseconds to wait for an operation to finish. */


static volatile flash_operation_t flash_operation = FLASH_NO_OPERATION;

static uint32_t const * address_of_page(uint16_t page_num);
static uint32_t const * address_of_word(uint32_t word_num);



volatile fs_ret_t handler_result = 0;

/**
 *@brief Function for handling fstorage events.
 */
static void fs_evt_handler(fs_evt_t const * const evt, fs_ret_t result)
{	handler_result = result;
	
    if (result == FS_SUCCESS) {	// Operation was successful!
		if(flash_get_operation() & FLASH_ERASE_OPERATION) {
			flash_operation &= ~FLASH_ERASE_OPERATION;
		}
		
		if(flash_get_operation() & FLASH_STORE_OPERATION) {
			flash_operation &= ~FLASH_STORE_OPERATION;			
		}
    } else {	// An error occurred during the operation!
	
		// Set the Error of the specific operation
		if(flash_get_operation() & FLASH_ERASE_OPERATION) {
			flash_operation &= ~FLASH_ERASE_OPERATION;
			flash_operation |= FLASH_ERASE_ERROR;
		}
		
		if(flash_get_operation() & FLASH_STORE_OPERATION) {
			flash_operation &= ~FLASH_STORE_OPERATION;		
			flash_operation |= FLASH_STORE_ERROR;
		}
	}
}





FS_REGISTER_CFG(fs_config_t fs_config) =
{
    .callback  = fs_evt_handler, /**< Function for event callbacks. */
    .num_pages = FLASH_NUM_PAGES,      /**< Number of physical flash pages required. */
    .priority  = 0x0F            /**< Priority for flash usage. */
};

ret_code_t flash_init(void) {
	
	// TODO: remove and put in main.c
	system_event_init();
	
	
	fs_ret_t status_init = fs_init();
	
	
	
	if(status_init != FS_SUCCESS) {
		return NRF_ERROR_INTERNAL; 
	}
	
	// Register the system event handler for flash, but only if we successfully initialized the fstorage module first.
	system_event_register_handler(fs_sys_event_handler);
	
	
	return NRF_SUCCESS;
}


/** @brief Retrieve the address of a page.
 * 
 * @param[in]	The page number for which the address should be retrieved.
 *
 * @retval 		Address of the specified page.
 */
static uint32_t const * address_of_page(uint16_t page_num)
{
    return fs_config.p_start_addr + (page_num * FLASH_PAGE_SIZE_WORDS);
}


/**@brief Retrieve the address of a word.
 *
 * @param[in]	The word number for which the address should be retrieved.
 *
 * @retval 		Address of the specified word.
 */
static uint32_t const * address_of_word(uint32_t word_num)
{
    return fs_config.p_start_addr + (word_num);
}







ret_code_t flash_erase_bkgnd(uint32_t page_num, uint16_t num_pages) {
	
	// Check if we have already an ongoing operation
	if((flash_get_operation() & FLASH_STORE_OPERATION) || (flash_get_operation() & FLASH_ERASE_OPERATION)) {
		return NRF_ERROR_BUSY;
	}
	
	flash_operation |= FLASH_ERASE_OPERATION;
	// Reset the Error flag (if it exists)
	flash_operation &= ~FLASH_ERASE_ERROR;
	
	// Call the fstorage library for the actual erase operation.
	fs_ret_t status_erase = fs_erase(&fs_config, address_of_page(page_num), num_pages, NULL);
	
	
	// Prepare the return value:
	ret_code_t ret = NRF_SUCCESS;
	if(status_erase != FS_SUCCESS) {
		
		// Reset the erase operation
		flash_operation &= ~FLASH_ERASE_OPERATION;
		
		if(status_erase == FS_ERR_QUEUE_FULL) {
			ret = NRF_ERROR_BUSY;
		} else if(status_erase == FS_ERR_NOT_INITIALIZED || status_erase == FS_ERR_INVALID_CFG){
			ret = NRF_ERROR_INTERNAL;
		} else { // FS_ERR_NULL_ARG || FS_ERR_INVALID_ARG || FS_ERR_INVALID_ADDR || FS_ERR_UNALIGNED_ADDR
			ret = NRF_ERROR_INVALID_PARAM;
		}
	}
	
	return ret;
}

ret_code_t flash_erase(uint32_t page_num, uint16_t num_pages) {
	ret_code_t ret = flash_erase_bkgnd(page_num, num_pages);
	
	// Return if erase operation was not started successfully
	if(ret != NRF_SUCCESS) {
		return ret;
	}
	
	// Wait for the erase operation to terminate with timeout check.
	uint64_t end_ms = systick_get_continuous_millis() + FLASH_OPERATION_TIMEOUT_MS;
	while(flash_get_operation() & FLASH_ERASE_OPERATION && systick_get_continuous_millis() < end_ms);
	
	if(flash_get_operation() & FLASH_ERASE_OPERATION) {
		// Reset the erase operation
		
		flash_operation &= ~FLASH_ERASE_OPERATION;
		return NRF_ERROR_TIMEOUT;
	}
	
	// Return an error if the erase operation was not successful.
	if(flash_get_operation() & FLASH_ERASE_ERROR) {
		return NRF_ERROR_TIMEOUT; 
	}
	
	return NRF_SUCCESS;
	
}



ret_code_t flash_store_bkgnd(uint32_t word_num, const uint32_t* p_words, uint16_t length_words) {	
	
	// Check if we have already an ongoing operation
	if((flash_get_operation() & FLASH_STORE_OPERATION) || (flash_get_operation() & FLASH_ERASE_OPERATION)) {
		return NRF_ERROR_BUSY;
	}
	
	if(p_words == NULL)
		return NRF_ERROR_INVALID_PARAM;
	
	
	flash_operation |= FLASH_STORE_OPERATION;
	// Reset the Error flag (if it exists)
	flash_operation &= ~FLASH_STORE_ERROR;
	
	// Call the fstorage library for the actual storage operation.
	fs_ret_t status_store = fs_store(&fs_config, address_of_word(word_num), p_words, length_words, NULL);
	
	

	// Prepare the return value:
	ret_code_t ret = NRF_SUCCESS;
	if(status_store != FS_SUCCESS) {
		
		// Reset the store operation
		flash_operation &= ~FLASH_STORE_OPERATION;
		
		if(status_store == FS_ERR_QUEUE_FULL) {
			ret = NRF_ERROR_BUSY;
		} else if(status_store == FS_ERR_NOT_INITIALIZED || status_store == FS_ERR_INVALID_CFG){
			ret = NRF_ERROR_INTERNAL;
		} else { // FS_ERR_NULL_ARG || FS_ERR_INVALID_ARG || FS_ERR_INVALID_ADDR || FS_ERR_UNALIGNED_ADDR
			ret = NRF_ERROR_INVALID_PARAM;
		}
	}
	
	return ret;
}

ret_code_t flash_store(uint32_t word_num, const uint32_t* p_words, uint16_t length_words) {
	// Call the non-blocking flash store function.
	ret_code_t ret = flash_store_bkgnd(word_num, p_words, length_words);
	
	// Return if store operation was not started successfully
	if(ret != NRF_SUCCESS) {
		return ret;
	}
	
	// Wait for the store operation to terminate with timeout check.
	uint64_t end_ms = systick_get_continuous_millis() + FLASH_OPERATION_TIMEOUT_MS;
	while(flash_get_operation() & FLASH_STORE_OPERATION && systick_get_continuous_millis() < end_ms);
	
	if(flash_get_operation() & FLASH_STORE_OPERATION) {
		// Reset the store operation
		flash_operation &= ~FLASH_STORE_OPERATION;
		return NRF_ERROR_TIMEOUT;
	}
	
	// Return an error if the store operation was not successful.
	if(flash_get_operation() & FLASH_STORE_ERROR) {
		return NRF_ERROR_TIMEOUT; 
	}
	
	return NRF_SUCCESS;
	
}

flash_operation_t flash_get_operation(void) {
	return flash_operation;
}


ret_code_t flash_read(uint32_t word_num, uint32_t* p_words, uint16_t length_words) {
	
	
	if(!nrf_drv_is_in_RAM(p_words))
		return NRF_ERROR_INVALID_PARAM;
	
	if(p_words == NULL)
		return NRF_ERROR_INVALID_PARAM;
	
	if(word_num + length_words > (FLASH_PAGE_SIZE_WORDS*FLASH_NUM_PAGES))
		return NRF_ERROR_INVALID_PARAM;
	
	for(uint32_t i = 0; i < length_words; i++) {
		p_words[i] = *(address_of_word(word_num + i));		
	}
	return NRF_SUCCESS;
}


uint32_t flash_get_page_size_words(void) {
	
	return FLASH_PAGE_SIZE_WORDS;	
}

uint32_t flash_get_page_number(void) {
	return FLASH_NUM_PAGES;
}




bool flash_selftest(void) {
	
	debug_log("FLASH: Started flash selftest...\n\r");
	
	debug_log("FLASH: Flash page addresses: From %p to %p\n\r", address_of_page(0), address_of_page(FLASH_NUM_PAGES-1));
	
	debug_log("FLASH: Flash word addresses: From %p to %p\n\r", address_of_word(0), address_of_word(flash_get_page_size_words()*flash_get_page_number()-1));
	
	debug_log("FLASH: Page size words: %u, Number of pages: %u\n\r", flash_get_page_size_words(), flash_get_page_number());
	
	
	// Just a dummy write to test erasing..
	uint32_t tmp = 0xABCD1234;		
	flash_store_bkgnd(0, &tmp, 1);

	// Wait for the store operation to terminate with timeout check.
	uint64_t end_ms = systick_get_continuous_millis() + FLASH_OPERATION_TIMEOUT_MS;
	while(flash_get_operation() & FLASH_STORE_OPERATION && systick_get_continuous_millis() < end_ms);
	if(flash_get_operation() & FLASH_STORE_OPERATION) {
		// Reset the store operation
		flash_operation &= ~FLASH_STORE_OPERATION;
		debug_log("FLASH: Flash store operation timed out!\n\r");
		return 0;
	}
	
	
//******************** Test erasing 2 pages *************************	
	ret_code_t ret = flash_erase_bkgnd(0, 2);
	debug_log("FLASH: Started erasing: Ret %d\n\r", ret);
	if(ret != NRF_SUCCESS) {
		debug_log("FLASH: Start erasing failed!\n\r");
		return 0;
	}
	
	flash_operation_t erase_operation = flash_get_operation();
	
	// Wait for the erase operation to terminate with timeout check.
	end_ms = systick_get_continuous_millis() + FLASH_OPERATION_TIMEOUT_MS;
	while(erase_operation & FLASH_ERASE_OPERATION && systick_get_continuous_millis() < end_ms) {
		erase_operation = flash_get_operation();
	}
	if(erase_operation & FLASH_ERASE_OPERATION) {
		// Reset the erase operation
		flash_operation &= ~FLASH_ERASE_OPERATION;
		debug_log("FLASH: Flash erase operation timed out!\n\r");
		return 0;
	}
	
	if(erase_operation & FLASH_ERASE_ERROR) {
		debug_log("FLASH: Erasing error!\n\r");
		return 0;
	}
	debug_log("FLASH: Erasing success!\n\r");
	
	
	
//******************** Test write a word *************************	
	#define FLASH_TEST_ADDRESS 0 //(flash_get_page_size_words()*flash_get_page_number())
	uint32_t write_word = 0x1234ABCD;	// Should be != 0xFFFFFFFF --> next test will assume this!
	
	ret = flash_store_bkgnd(FLASH_TEST_ADDRESS, &write_word, 1);
	debug_log("FLASH: Started storing to flash at word 0: 0x%X, Ret: %d\n\r", write_word, ret);
	if(ret != NRF_SUCCESS) {
		debug_log("FLASH: Start storing word failed!\n\r");
		return 0;
	}
	
	flash_operation_t store_operation = flash_get_operation();
	
	// Wait for the store operation to terminate with timeout check.
	end_ms = systick_get_continuous_millis() + FLASH_OPERATION_TIMEOUT_MS;
	while(store_operation & FLASH_STORE_OPERATION && systick_get_continuous_millis() < end_ms) {
		store_operation = flash_get_operation();
	}
	if(store_operation & FLASH_STORE_OPERATION) {
		// Reset the erase operation
		flash_operation &= ~FLASH_STORE_OPERATION;
		debug_log("FLASH: Flash store operation timed out!\n\r");
		return 0;
	}
	
	if(store_operation & FLASH_STORE_ERROR) {
		debug_log("FLASH: Storing error!\n\r");
		return 0;
	}
	
	
	// Read the word again:	
	uint32_t read_word;
	// Check if the stored word is the correct word!
	ret = flash_read(FLASH_TEST_ADDRESS, &read_word, 1);
	if(ret != NRF_SUCCESS) {
		debug_log("FLASH: Read failed!\n\r");
		return 0;
	}
	
	if(read_word != write_word) {
		debug_log("FLASH: Stored word is not the right word!\n\r");
		return 0;
	}
	
	
	debug_log("FLASH: Storing success!\n\r");

//******************** Test writing to the same position another value *************************
	write_word = 0xFFFFFFFF;	
	
	
	
	
	ret = flash_store(FLASH_TEST_ADDRESS + 100, &write_word, 1);
	//ret = flash_store_bkgnd(0, &write_word, 1);
	debug_log("FLASH: Storing to flash at word 0: 0x%X, Ret: %d\n\r", write_word, ret);

	if(ret != NRF_SUCCESS) {
		if(ret == NRF_ERROR_TIMEOUT) {
			debug_log("FLASH: Storing word timed out!\n\r");
		} else {
			debug_log("FLASH: Storing word failed!\n\r");
		}
		return 0;
	}
	
	// Check if the stored word is the correct word!
	ret = flash_read(FLASH_TEST_ADDRESS, &read_word, 1);
	if(ret != NRF_SUCCESS) {
		debug_log("FLASH: Read failed!\n\r");
		return 0;
	}	
	
	if(read_word == write_word) {
		debug_log("FLASH: Store word should actually fail!! Written: 0x%X, Read: 0x%X\n\r", write_word, read_word);
		return 0;
	}

	
	
	debug_log("FLASH: Store different word at same position behaves as expected!\n\r");
	
	
//******************** Test a overflowing write operation over 2 pages *************************
	#define WORD_NUMBER	5
	uint32_t write_address = (flash_get_page_size_words())-2;
	uint32_t write_words[WORD_NUMBER];
	ret = flash_store_bkgnd(write_address, write_words, WORD_NUMBER);
	debug_log("FLASH: Started storing words at address: %u, Ret: %d\n\r",write_address, ret);
	if(ret != NRF_SUCCESS) {
		debug_log("FLASH: Start storing words failed!\n\r");
		return 0;
	}
	
	store_operation = flash_get_operation();
	
	// Wait for the store operation to terminate with timeout check.
	end_ms = systick_get_continuous_millis() + FLASH_OPERATION_TIMEOUT_MS;
	while(store_operation & FLASH_STORE_OPERATION && systick_get_continuous_millis() < end_ms) {
		store_operation = flash_get_operation();
	}
	if(store_operation & FLASH_STORE_OPERATION) {
		// Reset the erase operation
		flash_operation &= ~FLASH_STORE_OPERATION;
		debug_log("FLASH: Flash store operation timed out!\n\r");
		return 0;
	}
	
	
	if(store_operation & FLASH_STORE_ERROR) {
		debug_log("FLASH: Storing words error!\n\r");
		return 0;
	}
	// Check if the stored words are correct!
	uint32_t read_words[WORD_NUMBER];
	ret = flash_read(write_address, read_words, WORD_NUMBER);
	if(ret != NRF_SUCCESS) {
		debug_log("FLASH: Read failed!\n\r");
		return 0;
	}	
	
	if(memcmp((uint8_t*)&write_words[0], (uint8_t*)&read_words[0], sizeof(uint32_t)*WORD_NUMBER) != 0) {
		debug_log("FLASH: Stored words are not the right words!\n\r");
		return 0;
	}
	
	debug_log("FLASH: Storing words success!\n\r");	
	
//******************* Test read to non RAM memory **************************************


	char* non_ram = "ABCD";
	uint32_t* p_non_ram = (uint32_t*) non_ram;
	
	ret = flash_read(0, p_non_ram, 1);
	if(ret == NRF_SUCCESS) {
		debug_log("FLASH: No RAM memory test failed!\n\r");
		return 0;
	}	


//******************* False address test **************************
	
	ret = flash_store(flash_get_page_size_words()*flash_get_page_number()-2, write_words, WORD_NUMBER);
	debug_log("FLASH: Test invalid address store, Ret: %d\n\r", ret);
	if(ret != NRF_ERROR_INVALID_PARAM) {
		debug_log("FLASH: Test invalid address store failed!\n\r");
		return 0;
	}
	
	ret = flash_read(flash_get_page_size_words()*flash_get_page_number()-2, write_words, WORD_NUMBER);
	debug_log("FLASH: Test invalid address read, Ret: %d\n\r", ret);
	if(ret != NRF_ERROR_INVALID_PARAM) {
		debug_log("FLASH: Test invalid address read failed!\n\r");
		return 0;
	}
	
//****************** Large data write ****************************	
	#define LARGE_WORD_NUMBER	100
	uint32_t large_words_address = 1234;
	uint32_t large_write_words[LARGE_WORD_NUMBER];
	uint32_t large_read_words[LARGE_WORD_NUMBER];
	for(uint32_t i = 0; i < LARGE_WORD_NUMBER; i++) {
		large_write_words[i] = i;
		large_read_words[i] = 0;	
	}
	
	ret = flash_erase(0, 15);
	debug_log("FLASH: Started erasing: Ret %d\n\r", ret);
	if(ret != NRF_SUCCESS) {
		debug_log("FLASH: Start erasing failed! @ %u\n\r", __LINE__);
		return 0;
	}
	
	ret = flash_store(large_words_address, large_write_words, LARGE_WORD_NUMBER);
	debug_log("FLASH: Test large words store, Ret: %d\n\r", ret);
	if(ret != NRF_SUCCESS) {
		debug_log("FLASH: Test large words store failed! @ %u\n\r", __LINE__);
		return 0;
	}
	
	ret = flash_read(large_words_address, large_read_words, LARGE_WORD_NUMBER);
	debug_log("FLASH: Test large words read, Ret: %d\n\r", ret);
	if(ret != NRF_SUCCESS) {
		debug_log("FLASH: Test large words read failed! @ %u\n\r", __LINE__);
		return 0;
	}
	
	if(memcmp((uint8_t*)&large_write_words[0], (uint8_t*)&large_read_words[0], sizeof(uint32_t)*LARGE_WORD_NUMBER) != 0) {
		debug_log("FLASH: Stored words are not the right words! @ %u\n\r", __LINE__);
		return 0;
	}
	
	debug_log("FLASH: Flash test successful!!\n\r");	
	
	
	return 1;
}