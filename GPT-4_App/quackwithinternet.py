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
from googlesearch import search
import re


load_dotenv()
terminalOutputs = []

def get_page_content(url):
    response = requests.get(url)
    if response.status_code == 200:
        return response.text
    else:
        return None

def extract_quoted_content(rawQuery):
    pattern = r'"([^"]*)"'
    matches = re.findall(pattern, rawQuery)
    return matches


def GPT4():
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
    conversation.append(reply)
    





def GPT4WithGoogle():
    googleSearch = "whats a good google search to answer this question?" + message
    messages.append( {"role": "user", "content": googleSearch} )
    response = openai.ChatCompletion.create(
        model="gpt-4",
        messages=messages,
        top_p=topP,
        temperature=temp,                                                   
        frequency_penalty=0.0,                                              
        presence_penalty=0.0 )
    
    rawQuery = response["choices"][0]["message"]["content"]
    conversation.append(rawQuery)

    goodQueryList = extract_quoted_content(rawQuery)
    goodQueryString = " ".join(goodQueryList)
    query = goodQueryString
    print(query)
    search_results = []
    webpage_content = []
    for results in search(query, tld="co.in", num=5, stop=5, pause=2):
        search_results.append(results)
        conversation.append(results)
        


    for url in search_results:
        content = get_page_content(url)
        if content is not None:
            webpage_content.append(content)
    
    webpage_string = '\n'.join(webpage_content)
    ##Problem. 
    updatedMessage = message + ". Use this data from the internet to answer the question: " + webpage_string
    messages.append( {"role": "user", "content": updatedMessage} )
    response = openai.ChatCompletion.create(
        model="gpt-4",
        messages=messages,
        top_p=topP,
        temperature=temp,                                                   
        frequency_penalty=0.0,                                              
        presence_penalty=0.0 )
    
    finalResponse = response["choices"][0]["message"]["content"]
    conversation.append(finalResponse)
    

    
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
menu = [["Settings", ["Change Temperature", "Change Top-P", "Change Recording Duration", "Change API Key"]], ["Debug", ["Show Terminal"]]]
title = sg.Text("To manually import a .txt file begin with R> \nTo manually import an audio file begin with L>", font=fontT)
folderlocation = os.path.dirname(os.path.realpath(__file__))
duck_img = sg.Image(f'{folderlocation}/duck.png', size=(130,130), enable_events=True, key="-DUCK-")
mic_button = sg.Button("Audio Input", key="-RECORD-")
send_button = sg.Button("Send")
clear_button = sg.Button("Clear")
quit_button = sg.Button("Quit")
internetsearch = sg.Checkbox("Access Internet (Uses way too many tokens and hardly works)", default=False, key="-SEARCH-")
#tempSlider = sg.Text("Temperature:", font=fontS), sg.Slider(range=(0, 10), default_value=5, orientation='h', size=(34, 10), font=fontS, key="-SLIDER1-")
#topPSlider = sg.Text("Top-P:", font=fontS), sg.Slider(range=(0, 10), default_value=5, orientation='h', size=(34, 10), font=fontS, key="-SLIDER2-")
#recordingDuration = sg.Text("Recording Duration:", font=fontS), sg.Slider(range=(0, 10), default_value=5, orientation='h', size=(34, 10), font=fontS, key="-SLIDER3-")
mainInput = sg.Multiline("", size=(100,5), key="-INPUT-", font=fontB)
secondaryInput = sg.Input(enable_events=True, key='-IN-',font=fontS, expand_x=True), sg.FileBrowse()
mainOutput = sg.Multiline("", size=(100,25), key="-OUTPUT-", font=fontB)
TempAsk = 5
TopPAsk = 5
waddle_position = 0
results = ""


#define the layout of the GUI
layout = [[sg.Menu(menu)],
        [title, sg.Column([[duck_img]], justification='right')],
        #[tempSlider, topPSlider, recordingDuration]
        [mainInput],
        [secondaryInput],
        [internetsearch, send_button, clear_button, quit_button, sg.Column([[mic_button]], justification='right')],
        [mainOutput]
    ]



# Create the GUI window
window = sg.Window("GPT-4", layout, size=(700, 700))

# Start the event loop
while True:
    
    event, values = window.read()
    if event == "Quit" or event == sg.WIN_CLOSED:
        break
    
    while True: 
        if event == "Change API Key":
            APIASK = sg.popup_get_text("Please enter your API Key", title="API Key")
            print("Key = ", APIASK)
            if APIASK != None:
                OPENAI_API_KEY = APIASK
            break
        elif event == "-RECORD-":
            ## RECORD AUDIO and make it into a temporary .mp3 file

            break
        elif event == "Show Terminal":
            TerminalContent = '\n'.join(terminalOutputs)
            TerminalPopup = sg.popup_scrolled(TerminalContent + "this doesnt work yet, please help", size=(100, 100), title="Terminal", font=fontS)
            print("Terminal Opened")
            break
        elif event == "Change Temperature":
            TempAsk = sg.popup_get_text("Enter Temperature value between 1-10", title="Temperature")
            if TempAsk == None:
                TempAsk = 5
            print("Temperature Changed")
            break
        elif event == "Change Top-P":
            TopPAsk = sg.popup_get_text("Enter Top-P value between 1-10", title="Top-P")
            if TopPAsk == None:
                TopPAsk = 5
            print("Top-P Changed")
            break
        elif event == "Change Recording Duration":
            RecordingDurationAsk = sg.popup_get_text("Enter Recording Duration ", title="Recording Duration")
            if RecordingDurationAsk == None:
                RecordingDurationAsk = 5
            recordingDurationSeconds = RecordingDurationAsk
            print("Recording Duration Changed")
            break
        else:
            break
            
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
    
    
    temp = (float(TempAsk) * 0.1)
    topP = (float(TopPAsk) * 0.1)

    
    
    if event == "Send":
        if values["-SEARCH-"] == True:
            GPT4WithGoogle()
            conversationString = "\n \n".join(conversation)

            window["-OUTPUT-"].update(conversationString)
            window["-IN-"].update("")
            window["-INPUT-"].update("")
        else:
            GPT4()
            conversationString = "\n \n".join(conversation)

            window["-OUTPUT-"].update(conversationString)
            window["-IN-"].update("")
            window["-INPUT-"].update("")