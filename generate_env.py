Import("env")
import os
import shutil

def generate_env_file():
    # Ensure data directory exists
    data_dir = os.path.join(env.get('PROJECT_DIR'), 'data')
    os.makedirs(data_dir, exist_ok=True)
    
    # Path to env files
    env_example = os.path.join(env.get('PROJECT_DIR'), '.env.example')
    env_file = os.path.join(env.get('PROJECT_DIR'), '.env')
    env_data = os.path.join(data_dir, '.env')
    
    # If .env exists, copy it to data directory
    if os.path.exists(env_file):
        shutil.copy2(env_file, env_data)
        print("Copied .env to data directory")
    # If not, copy example file
    elif os.path.exists(env_example):
        shutil.copy2(env_example, env_data)
        print("Warning: Using .env.example! Create .env file for production.")
    else:
        print("Error: Neither .env nor .env.example found!")
        env.Exit(1)

generate_env_file()