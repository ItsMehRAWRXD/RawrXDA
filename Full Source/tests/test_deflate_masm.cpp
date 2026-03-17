#include "logging/logger.h"

// External function from the assembly file
extern "C" int deflate_brutal_masm(unsigned char* out, const unsigned char* in, int in_len, int* out_len);

int main() {
    Logger logger("TestDeflateMasm");

    // Dummy data for testing
    const unsigned char input_data[] = "This is a test string for brutal deflation.";
    int input_len = sizeof(input_data);
    unsigned char output_data[1024];
    int output_len = 0;

    logger.info("Calling deflate_brutal_masm...");

    // Call the assembly function
    int result = deflate_brutal_masm(output_data, input_data, input_len, &output_len);

    if (result == 0) {
        logger.info("deflate_brutal_masm succeeded.");
        logger.info("Original size: {}", input_len);
        logger.info("Compressed size: {}", output_len);
    } else {
        logger.error("deflate_brutal_masm failed with code: {}", result);
    }

    return 0;
}
