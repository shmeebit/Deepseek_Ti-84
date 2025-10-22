# TI-84 Plus CE Python Programs

This directory contains Python programs for the TI-84 Plus CE calculator.

## ti32_ai.py - AI Assistant

Main program that communicates with the ESP32-S3 to query DeepSeek AI via USB.

### Installation

1. **Transfer to Calculator:**
   - Connect your TI-84 Plus CE to your computer via USB
   - Use TI Connect CE software or similar to transfer `ti32_ai.py` to your calculator
   - Alternatively, use the calculator's file transfer feature

2. **Run the Program:**
   - On your calculator, press the `prgm` button
   - Navigate to the Python programs
   - Select `ti32_ai` and press Enter

### Usage

The program provides a menu interface:

1. **Ask a question** - Ask any general question to the AI
2. **Math solver** - Get help solving math problems
3. **Quick facts** - Get interesting facts about science, history, or math
4. **Exit** - Quit the program

### Communication Protocol

The program uses TI-BASIC string variables to communicate with the ESP32-S3:

- **Str0**: Status/trigger variable
  - Set to `"GO"` by calculator to trigger a query
  - ESP32 sets to `"WAIT"` while processing
  - ESP32 sets to `"DONE"` when complete or `"ERROR"` on failure
  
- **Str1**: Question/input from calculator
  - Calculator writes the question here before triggering
  
- **Str2**: Answer/output from ESP32
  - ESP32 writes the AI response here

### Requirements

- TI-84 Plus CE (Python Edition)
- ESP32-S3 running the ti32 firmware (see `../esp32s3/`)
- USB OTG cable to connect calculator to ESP32-S3
- Node.js server running Ollama (see `../server/`)

### Troubleshooting

**"Timeout waiting for response"**
- Check that ESP32-S3 is powered and running
- Verify USB connection between calculator and ESP32
- Check ESP32 serial monitor for connection status

**"ESP32 reported an error"**
- Check ESP32 serial monitor for error details
- Verify WiFi connection on ESP32
- Ensure server is running and accessible

**"ti_system not available"**
- This is normal when running on a PC (for testing)
- The program will use mock functions for development

### Customization

You can modify the program to:
- Add more menu options
- Change the timeout duration (default: 30 seconds)
- Customize the display format
- Add additional quick-access questions

### Example Questions

- "What is the derivative of x^2?"
- "Explain the Pythagorean theorem"
- "How do I solve quadratic equations?"
- "What is the value of pi to 10 decimal places?"
- "Explain Newton's laws of motion"

## Tips

- Keep questions concise for faster responses
- Responses are limited to 255 characters to fit calculator string limits
- The AI is optimized for educational content and math help
- Press the calculator's `ON` button to interrupt a long wait

## Development

To test the program on your PC before transferring to the calculator:

```bash
python ti32_ai.py
```

The program will detect it's not running on actual calculator hardware and use mock functions.
