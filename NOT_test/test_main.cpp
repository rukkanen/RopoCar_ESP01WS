// #include <gtest/gtest.h>
// #include <gmock/gmock.h>

// // Include the main.cpp file
// #include "../src/main.cpp"

// // Test case for handleRoot function
// TEST(MainTest, HandleRootTest) {
//     // Create a mock server object
//     class MockServer : public ESP8266WebServer {
//     public:
//         MOCK_METHOD(void, send, void(int, const String&, const String&));
//     };

//     // Create an instance of the mock server
//     MockServer server;

//     // Set the expected behavior of the mock server
//     EXPECT_CALL(server, send(200, "text/html", _));

//     // Call the handleRoot function
//     handleRoot();

//     // Verify that the expected behavior of the mock server was called
//     testing::Mock::VerifyAndClearExpectations(&server);
// }

// // Test case for handleModeChange function
// TEST(MainTest, HandleModeChangeTest) {
//     // Create a mock server object
//     class MockServer : public ESP8266WebServer {
//     public:
//         MOCK_METHOD(void, send, void(int, const String&, const String&));
//         String uri() { return "/mode/toy"; }
//     };

//     // Create an instance of the mock server
//     MockServer server;

//     // Call the handleModeChange function
//     handleModeChange();

//     // Verify that the mode was changed to "toy"
//     EXPECT_EQ(mode, "toy");
// }

// // Test case for handleCamera function
// TEST(MainTest, HandleCameraTest) {
//     // Create a mock server object
//     class MockServer : public ESP8266WebServer {
//     public:
//         MOCK_METHOD(void, send, void(int, const String&, const String&));
//     };

//     // Create an instance of the mock server
//     MockServer server;

//     // Set the expected behavior of the mock server
//     EXPECT_CALL(server, send(200, "image/jpeg", _));

//     // Call the handleCamera function
//     handleCamera();

//     // Verify that the expected behavior of the mock server was called
//     testing::Mock::VerifyAndClearExpectations(&server);
// }

// // Run all the tests
// int main(int argc, char** argv) {
//     testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }