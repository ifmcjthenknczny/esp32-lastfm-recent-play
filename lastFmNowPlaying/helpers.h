#ifndef HELPERS_H
#define HELPERS_H

#include <WiFiClientSecure.h>

const unsigned long HTTPS_CONNECT_TIMEOUT = 5000;
const unsigned long HTTPS_HEADER_TIMEOUT = 10000; // Timeout waiting for headers after connecting
const unsigned long HTTPS_BODY_TIMEOUT = 10000;   // Timeout waiting for body data after headers

String fetchJsonOverHttps(const char* host, uint16_t port, const char* path) {
    WiFiClientSecure client; // Use secure client for HTTPS
    String responseBody = ""; // Initialize empty response body
    bool httpOk = false;

    // Optional: Allow insecure connections for testing ONLY if needed
    // Useful if device time is wrong or certificate store is missing/old.
    // client.setInsecure();

    Serial.printf("Manual HTTPS: Connecting to %s:%u...\n", host, port);
    if (!client.connect(host, port, HTTPS_CONNECT_TIMEOUT)) {
        Serial.println("Manual HTTPS: Connection failed!");
        return responseBody;
    }
    Serial.println("Manual HTTPS: Connected.");

    // Construct the HTTP GET request manually
    String request = String("GET ") + path + " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" +
                     "Accept: application/json\r\n" + // Request JSON
                     "Connection: close\r\n\r\n";     // Close connection after response

    Serial.print("Manual HTTPS: Sending request:\n");
    Serial.print(request); // Log the request being sent
    client.print(request);

    // --- Wait for response headers ---
    unsigned long headerStartTime = millis();
    while (!client.available() && millis() - headerStartTime < HTTPS_HEADER_TIMEOUT) {
        delay(50); // Wait polling
    }

    if (!client.available()) {
        Serial.println("Manual HTTPS: No response headers received (timeout)!");
        client.stop();
        return responseBody;
    }

    // --- Read HTTP Status Line & Headers ---
    String statusLine = "";
    bool headersDone = false;
    headerStartTime = millis(); // Reset timeout for reading headers phase

    while (client.connected() || client.available()) {
         if (millis() - headerStartTime > HTTPS_HEADER_TIMEOUT) {
            Serial.println("Manual HTTPS: Timeout reading headers!");
            httpOk = false;
            break;
        }

        if (client.available()) {
            String line = client.readStringUntil('\n');
            line.trim(); // Remove trailing \r

            if (statusLine == "") { // First line is status
                statusLine = line;
                Serial.print("Manual HTTPS Status: ");
                Serial.println(statusLine);
                if (statusLine.startsWith("HTTP/1.1 200 OK")) {
                    httpOk = true;
                } else {
                    // Handle non-200 status - could include redirects here if needed, but complex
                    Serial.println("Manual HTTPS: Non-200 status received.");
                    httpOk = false; // Ensure flag is false
                    // Keep reading headers to properly consume server output, but don't parse body
                }
            }

            // Empty line indicates end of headers
            if (line.length() == 0) {
                headersDone = true;
                break; // Exit header reading loop
            }
             headerStartTime = millis(); // Reset timeout while receiving lines
        } else {
            delay(10); // Wait if buffer empty but still connected
        }
    }

    if (!headersDone) {
         Serial.println("Manual HTTPS: Did not reach end of headers (disconnected or timeout).");
         httpOk = false; // Cannot proceed if headers didn't finish
    }

    // --- Read Body (only if Status was OK and headers finished) ---
    if (httpOk && headersDone) {
        Serial.println("Manual HTTPS: Reading body...");
        unsigned long bodyStartTime = millis();
        // Read the rest of the response into the body string
        // Warning: Potentially large memory usage!
        // Alternatives involve reading chunk by chunk or directly into JSON parser (more complex manually)
        responseBody = client.readString(); // Read remainder

        if (millis() - bodyStartTime > HTTPS_BODY_TIMEOUT){
             Serial.println("Manual HTTPS: Warning - Reading body might have timed out (check content).");
             // Body might be incomplete, but we return what we got
        }
        Serial.printf("Manual HTTPS: Body length: %d\n", responseBody.length());
    } else {
         Serial.println("Manual HTTPS: Skipping body reading due to non-200 status or header error.");
         responseBody = ""; // Ensure body is empty if status wasn't OK
    }


    Serial.println("Manual HTTPS: Stopping client.");
    client.stop();
    return responseBody;
}

#endif