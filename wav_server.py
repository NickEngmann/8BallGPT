from flask import Flask, request
import os
from datetime import datetime

app = Flask(__name__)

# Create uploads directory if it doesn't exist
UPLOAD_DIR = 'wav_uploads'
os.makedirs(UPLOAD_DIR, exist_ok=True)

@app.route('/upload_wav', methods=['POST'])
def upload_wav():
    if 'file' not in request.files:
        return 'No file part', 400
    
    file = request.files['file']
    if file.filename == '':
        return 'No selected file', 400
    
    # Create filename with timestamp
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = f'recording_{timestamp}.wav'
    
    # Save the file
    filepath = os.path.join(UPLOAD_DIR, filename)
    file.save(filepath)
    print(f"Saved file: {filepath}")
    
    return 'File uploaded successfully', 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)