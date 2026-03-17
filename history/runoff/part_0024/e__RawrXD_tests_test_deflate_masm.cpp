#include <iostream>

// External function from the assembly file
extern "C" int deflate_brutal_masm(unsigned char* out, const unsigned char* in, int in_len, int* out_len);

int main() {
    // Dummy data for testing
    const unsigned char input_data[] = "This is a test string for brutal deflation.";
    int input_len = sizeof(input_data);
    unsigned char output_data[1024]; // Make sure this is large enough
    int output_len = 0;

    std::cout << "Calling deflate_brutal_masm..." << std::endl;

    // Call the assembly function
    int result = deflate_brutal_masm(output_data, input_data, input_len, &output_len);

    if (result == 0) {
        std::cout << "deflate_brutal_masm succeeded." << std::endl;
        std::cout << "Original size: " << input_len << std::endl;
        std::cout << "Compressed size: " << output_len << std::endl;
    } else {
        std::cout << "deflate_brutal_masm failed with code: " << result << std::endl;
    }

    return 0;
}
