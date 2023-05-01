import openai
import os
import json
import requests
import webbrowser
import random 
import docx2txt
import PySimpleGUI as sg
import sounddevice as sd
from scipy.io.wavfile import write
import wavio as wv
from pydub import AudioSegment
from dotenv import load_dotenv

load_dotenv()

###CALL DALL-E API TO REQUEST PHOTOS
def generate_dall_e_image(prompt):
    headers = {
        'Content-Type': 'application/json',
        'Authorization': f'Bearer {openai.api_key}'
    }
    data = {
        'model': 'image-alpha-001',
        'prompt': f'{prompt}',
        'num_images': 1,
        'size': '1024x1024'
    }
    url = 'https://api.openai.com/v1/images/generations'

    # Send the request to the DALL-E API
    response = requests.post(url, headers=headers, data=json.dumps(data))
    response_json = response.json()

    # Check if the response was successful
    if response.status_code == 200 and 'data' in response_json and len(response_json['data']) > 0:
        return response_json['data'][0]['url']
    else:
        return None

def whisper_transcript():
        strippedLocation = message.split("L>")[1].strip()
        audioFile = open(strippedLocation, "rb")
        transcript = openai.Audio.transcribe("whisper-1", audioFile)
        transcript = str(transcript)
        
        #remove the {} from the start and end of the transcript
        finalTranscript = transcript[12:-1]
        message = "If possible, use this as a primary source for any further questions:" + finalTranscript


##extra encouragement;)
messages = [
    {"role": "system", "content": "Hello, what can I do for you today?"},
    {"role": "user", "content": "You are a helpful assistant."},
    {"role": "system", "content": "Thank you, I try my best."}
]
conversation = []
system_msg = "You are an AI."
OPENAI_API_KEY = os.getenv("OPENAI_API_KEY")
openai.api_key = OPENAI_API_KEY

#customize the GUI
sg.theme("DarkGreen2")
fontT = ("Arial", 20)
fontB = ("Arial", 15)
fontS = ("Arial", 10)
folderlocation = os.path.dirname(os.path.realpath(__file__))
duck_img = sg.Image(f'{folderlocation}/duckwaddle1.png', size=(200,200), enable_events=True, key="-DUCK-")
mic_button = sg.Button("Audio Input", key="-RECORD-")
send_button = sg.Button("Send")
clear_button = sg.Button("Clear")
quit_button = sg.Button("Quit")
waddle_position = 0

#define the layout of the GUI
layout = [
        [sg.Text("To manually import a .txt file begin with R> \nTo manually import an audio file begin with L>", font=fontT), sg.Column([[duck_img]], justification='right')],
        [sg.Text("Temperature:", font=fontS), sg.Slider(range=(0, 10), default_value=5, orientation='h', size=(34, 10), font=fontS, key="-SLIDER1-")],
        [sg.Text("Top-P:           ", font=fontS), sg.Slider(range=(0, 10), default_value=5, orientation='h', size=(34, 10), font=fontS, key="-SLIDER2-")],
        [sg.Text("Recording Duration:", font=fontS), sg.Slider(range=(0, 10), default_value=5, orientation='h', size=(34, 10), font=fontS, key="-SLIDER3-")],
        [sg.Multiline("", size=(100,5), key="-INPUT-", font=fontB)],
        [sg.Input(enable_events=True, key='-IN-',font=fontS, expand_x=True), sg.FileBrowse()],
        [send_button, clear_button, quit_button, sg.Column([[mic_button]], justification='right')],
        [sg.Multiline("", size=(100,25), key="-OUTPUT-", font=fontB)]
    ]



# Create the GUI window
window = sg.Window("GPT-4", layout, size=(700, 700))

# Start the event loop
while True:
    
    event, values = window.read()
    if event == "Quit" or event == sg.WIN_CLOSED:
        break
    

    ###BUTTON EVENTS###

    if event == "-DUCK-":
        if waddle_position == 0:
            waddle_position = 1
            duck_img.update(filename=f'{folderlocation}\duckwaddle2.png')
        elif waddle_position == 1:
            waddle_position = 0
            duck_img.update(filename=f'{folderlocation}\duckwaddle1.png')
        



    if event == "-RECORD-":
        ## RECORD AUDIO and make it into a temporary .mp3 file
        freq = 44100
        duration = values["-SLIDER3-"]
        
        recording = sd.rec(int(duration * freq), samplerate=freq, channels=2)
        
        duck_img.update(filename=f'{folderlocation}\duckspeak.png')
        sd.wait()
        duck_img.update(filename=f'{folderlocation}\duckwaddle1.png')
        
        write("output.wav", freq, recording)
        os.open("output.wav", os.O_RDONLY)
        
        sound = AudioSegment.from_wav('output.wav')
        sound.export('finalOutput.mp3', format='mp3')
        
        print(sound)

        
    
    docURL = values["-IN-"]
    if event == "-IN-":
        if ".mp3" in docURL:
            message = ("L>" + docURL)
        elif ".txt" in docURL:
            message = ("R>" + docURL)
        elif ".docx" in docURL:
            convFile = docx2txt.process(docURL)
            txtContentCV = convFile.replace('\n',' ')
            message = "If possible, use this as a primary source for any further questions:" + txtContentCV
            
    else:
        messagePreRep = values["-INPUT-"]
        message = messagePreRep.replace('\n',' ')
        conversation.append(message)
        
    if event == "Clear":
        window["-INPUT-"].update("")
        window["-OUTPUT-"].update("")
        conversation = []
        messages = []
        messages.append( {"role": "user", "content": "Hello, what can I do for you today?"} )
        messages.append( {"role": "system", "content": "You are a helpful assistant."} )
        messages.append( {"role": "user", "content": "Thank you, I try my best."} )
        continue
    
    
    temp = (values["-SLIDER1-"] * 0.1)
    topP = (values["-SLIDER2-"] * 0.1)

    #OPEN TEXT FILES
    if "R>" in message:
        sourceLocation = message.split("R>")[1].strip()
        extension = sourceLocation[len(sourceLocation)-5:]

        with open(sourceLocation) as file:
            txtContent = file.read().replace('\n',' ')
        message = "If possible, use this as a primary source for any further questions:" + txtContent
    
    #TRANSCRIBE AUDIO INTO PROMPT
    if "L>" in message:
        whisper_transcript()
    
    #GPT-4 api call
    messages.append( {"role": "user", "content": message} )
    response = openai.ChatCompletion.create(
        model="gpt-4",
        messages=messages,
        top_p=topP,
        temperature=temp,
        frequency_penalty=0.0,
        presence_penalty=0.0 )
    
    reply = response["choices"][0]["message"]["content"]
    messages.append({"role": "assistant", "content": reply})
    print("\n" + reply + "\n")
    
    #output reply to GUI
    conversation.append(reply)
    conversationString = "\n \n".join(conversation)

    window["-OUTPUT-"].update(conversationString)
    window["-IN-"].update("")
    
    #Create photo with DALL-E
    if "~" in reply:
        split_prompt = reply.split("~")[1].strip()
        image_url = generate_dall_e_image(split_prompt)
        print(image_url)
        #Open photo automatically
        if image_url != None:
            webbrowser.open(image_url)
            
            
