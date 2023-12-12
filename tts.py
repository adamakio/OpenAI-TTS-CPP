import os
import io
import soundfile as sf
import sounddevice as sd

from openai import OpenAI
from dotenv import load_dotenv

client = OpenAI(api_key=os.environ["OPENAI_API_KEY"])
for i in range(1, 10): 
  spoken_response = client.audio.speech.create(
    model="tts-1-hd",
    voice="alloy",
    response_format="mp3",
    input="Using the right rudder in climb may be easy to forget when you're overloaded with other tasks. You need to 'step on the ball'"
  )

  print("Spoken response: ", spoken_response)

  # Open a file in binary mode for writing
  with open("alloy" + str(i) + '.mp3', 'wb') as f:
      for chunk in spoken_response.iter_bytes(chunk_size=4096):
          print(len(chunk))
          # Write the chunk to the file
          f.write(chunk)

buffer = io.BytesIO()
for chunk in spoken_response.iter_bytes(chunk_size=4096):
  print(len(chunk))
  buffer.write(chunk)
buffer.seek(0)

with sf.SoundFile(buffer, 'r') as sound_file:
  data = sound_file.read(dtype='int16')
  print("Sound file: ", sound_file)
  print("Sample rate: ", sound_file.samplerate)
  print("Frames: ", sound_file.frames)
  print("Channels: ", sound_file.channels)
 
  sd.play(data, sound_file.samplerate)
  sd.wait()