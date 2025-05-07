using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using Newtonsoft.Json.Linq;
using SIPSorcery.Net;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using WebSocketSharp;

namespace uwp_webrtc
{
    class OfferMessage
    {
        public readonly string type = "offer";
        public RTCSessionDescriptionInit offer;        

        public OfferMessage(RTCSessionDescriptionInit offer)
        {
            this.offer = offer;
        }
    }

    class AnswerMessage
    {
        public readonly string type = "answer";
        public RTCSessionDescriptionInit answer;        

        public AnswerMessage(RTCSessionDescriptionInit answer)
        {
            this.answer = answer;
        }
    }

    class IceCandidateMessage
    {
        public readonly string type = "candidate";
        public RTCIceCandidateInit candidate;        

        public IceCandidateMessage(RTCIceCandidate candidate)
        {
            this.candidate = new RTCIceCandidateInit
            {
                candidate = candidate.candidate,
                sdpMid = candidate.sdpMid,
                sdpMLineIndex = candidate.sdpMLineIndex,
                usernameFragment = candidate.usernameFragment,
            };
        }

        public IceCandidateMessage() { }
    }

    class SignalingSocket
    {
        private WebSocket ws;
        public delegate void ReceiveSessionDescription(RTCSessionDescriptionInit answer);
        public delegate void ReceiveIceCandidate(RTCIceCandidateInit candidate);

        public event ReceiveSessionDescription OnAnswer;
        public event ReceiveSessionDescription OnOffer;
        public event ReceiveIceCandidate OnIceCandidate;

        private readonly JsonSerializerSettings settings = new JsonSerializerSettings
        {
            Converters = new List<JsonConverter> { new StringEnumConverter() }
        };  
        private readonly string url;

        public SignalingSocket(string url)
        {
            this.url = url;
            ws = new WebSocket(url);
        }

        public void Connect()
        {
            ws.Connect();
            if (!ws.IsAlive)
            {
                throw new Exception("Socket not connected!");
            }
            ws.OnError += (sender, args) =>
            {
                throw new Exception("Socket Disconetec!");
            };
            ws.OnMessage += HandleMessage;
        }

        private void HandleMessage(object sender, MessageEventArgs e)
        {
            JObject msg = JObject.Parse(e.Data);
            string type = msg["type"]?.Value<string>();
            if (type.IsNullOrEmpty())
            {
                Debug.WriteLine("Invalid Message: missing type");
                return;
            }

            switch (type)
            {
                case "offer":
                    OfferMessage offerMessage = JsonConvert.DeserializeObject<OfferMessage>(e.Data);
                    OnOffer?.Invoke(offerMessage.offer);
                    break;
                case "answer":
                    AnswerMessage answerMessage = JsonConvert.DeserializeObject<AnswerMessage>(e.Data);
                    OnAnswer?.Invoke(answerMessage.answer);
                    break;
                case "candidate":
                    IceCandidateMessage candidateMessage = JsonConvert.DeserializeObject<IceCandidateMessage>(e.Data);
                    OnIceCandidate?.Invoke(candidateMessage.candidate);
                    break;
            }
        }

        public void SendAnswer(RTCSessionDescriptionInit answer)
        {
            AnswerMessage message = new AnswerMessage(answer);
            string serialized = JsonConvert.SerializeObject(message, settings);

            ws.Send(serialized);
        }

        public void SendOffer(RTCSessionDescriptionInit offer)
        {
            OfferMessage message = new OfferMessage(offer);
            string serialized = JsonConvert.SerializeObject(message, settings);

            ws.Send(serialized);
        }

        public void SendIceCandidate(RTCIceCandidate candidate)
        {
            IceCandidateMessage message = new IceCandidateMessage(candidate);
            string serialized = JsonConvert.SerializeObject(message, settings);

            ws.Send(serialized);
        }

        public void Disconnect() {
            ws.Close();
        }
    }
}
