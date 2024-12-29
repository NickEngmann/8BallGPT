// Val.town function to process audio and get GPT response
export async function processAudioAndGetResponse(req) {
  const debugInfo = {
    steps: [],
    errors: [],
    timestamps: {},
    requestInfo: {},
    apiResponses: {},
  };

  try {
    debugInfo.timestamps.start = new Date().toISOString();

    // Check request method
    debugInfo.requestInfo.method = req.method;
    if (req.method !== "POST") {
      throw new Error(`Invalid request method: ${req.method}`);
    }

    // Extract and validate the WAV file
    debugInfo.steps.push("Extracting form data");
    const formData = await req.formData();
    const audioFile = formData.get("file");

    if (!audioFile) {
      throw new Error("No audio file provided in form data");
    }

    debugInfo.requestInfo.fileInfo = {
      type: audioFile.type,
      size: audioFile.size,
      name: audioFile.name,
    };
    debugInfo.steps.push(`Audio file extracted successfully: ${audioFile.size} bytes`);

    // Validate OpenAI API key
    if (!process.env.OPENAI_API_KEY) {
      throw new Error("OpenAI API key not configured");
    }

    // Function to handle retrying requests
    async function retryRequest(fn, maxRetries = 3, delay = 1000) {
      for (let i = 0; i < maxRetries; i++) {
        try {
          return await fn();
        } catch (error) {
          if (error.status === 429 && i < maxRetries - 1) {
            debugInfo.steps.push(`Rate limited, attempt ${i + 1}/${maxRetries}. Waiting ${delay}ms...`);
            await new Promise(resolve => setTimeout(resolve, delay));
            delay *= 2; // Exponential backoff
            continue;
          }
          throw error;
        }
      }
    }

    // Send to Whisper API with retry logic
    debugInfo.steps.push("Sending to Whisper API");
    debugInfo.timestamps.whisperStart = new Date().toISOString();

    const whisperFormData = new FormData();
    whisperFormData.append("file", audioFile);
    whisperFormData.append("model", "whisper-1");

    const whisperResponse = await retryRequest(async () => {
      const response = await fetch("https://api.openai.com/v1/audio/transcriptions", {
        method: "POST",
        headers: {
          "Authorization": `Bearer ${process.env.OPENAI_API_KEY}`,
        },
        body: whisperFormData,
      });

      if (!response.ok) {
        const errorText = await response.text();
        debugInfo.apiResponses.whisperError = {
          status: response.status,
          statusText: response.statusText,
          error: errorText,
        };
        const error = new Error(`Whisper API error: ${response.status} ${response.statusText}`);
        error.status = response.status;
        throw error;
      }

      return response;
    });

    const transcription = await whisperResponse.json();
    debugInfo.timestamps.whisperEnd = new Date().toISOString();
    debugInfo.apiResponses.whisperSuccess = {
      transcriptionLength: transcription.text?.length || 0,
      transcriptionText: transcription.text,
    };
    debugInfo.steps.push("Whisper transcription successful");

    if (!transcription.text) {
      throw new Error("No transcription text in Whisper response");
    }

    // Send to ChatGPT with retry logic
    debugInfo.steps.push("Sending to ChatGPT API");
    debugInfo.timestamps.gptStart = new Date().toISOString();

    const gptPayload = {
      model: "gpt-4",
      messages: [
        {
          role: "system",
          content:
            "You are the Magic 8 Ball. Provide mystical, fortune-telling responses to questions, similar to the classic toy. Keep responses concise, under 5 words. If the user didn't ask a question, still give vague advice",
        },
        {
          role: "user",
          content: transcription.text,
        },
      ],
      max_tokens: 100,
    };

    debugInfo.apiResponses.gptRequest = gptPayload;

    const gptResponse = await retryRequest(async () => {
      const response = await fetch("https://api.openai.com/v1/chat/completions", {
        method: "POST",
        headers: {
          "Authorization": `Bearer ${process.env.OPENAI_API_KEY}`,
          "Content-Type": "application/json",
        },
        body: JSON.stringify(gptPayload),
      });

      if (!response.ok) {
        const errorText = await response.text();
        debugInfo.apiResponses.gptError = {
          status: response.status,
          statusText: response.statusText,
          error: errorText,
        };
        const error = new Error(`GPT API error: ${response.status} ${response.statusText}`);
        error.status = response.status;
        throw error;
      }

      return response;
    });

    const gptResult = await gptResponse.json();
    debugInfo.timestamps.gptEnd = new Date().toISOString();
    debugInfo.steps.push("ChatGPT response received");

    if (!gptResult.choices || !gptResult.choices[0]) {
      throw new Error("No choices in GPT response");
    }

    // Calculate timing information
    debugInfo.timestamps.end = new Date().toISOString();
    const timings = {
      whisperDuration: new Date(debugInfo.timestamps.whisperEnd) - new Date(debugInfo.timestamps.whisperStart),
      gptDuration: new Date(debugInfo.timestamps.gptEnd) - new Date(debugInfo.timestamps.gptStart),
      totalDuration: new Date(debugInfo.timestamps.end) - new Date(debugInfo.timestamps.start),
    };

    // Return success response with debug info
    return new Response(
      JSON.stringify({
        success: true,
        transcription: transcription.text,
        response: gptResult.choices[0].message.content,
        debug: {
          ...debugInfo,
          timings,
        },
      }),
      {
        headers: { "Content-Type": "application/json" },
      },
    );
  } catch (error) {
    debugInfo.errors.push({
      message: error.message,
      stack: error.stack,
      timestamp: new Date().toISOString(),
    });

    return new Response(
      JSON.stringify({
        success: false,
        error: error.message,
        debug: debugInfo,
      }),
      {
        status: 500,
        headers: { "Content-Type": "application/json" },
      },
    );
  }
}