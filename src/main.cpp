#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <algorithm>
#include <random>
#include <cmath>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <hueplusplus/Bridge.h>
#include <hueplusplus/Light.h>
#include <hueplusplus/LinHttpHandler.h>

#include <print>
#include <ranges>
#include <csignal>
#include <atomic>

#include <hueplusplus/Group.h>
#include <hueplusplus/EntertainmentMode.h>
#include <hueplusplus/StateTransaction.h>
#include <hueplusplus/BridgeConfig.h>

#include <cstdio>
#include <vector>
#include "CurlHttpHandler.h"

#include <pulse/simple.h>
#include <pulse/error.h>

// SFML graafista oskilloskooppia varten
#include <SFML/Graphics.hpp>

using json = nlohmann::json;
using namespace hueplusplus;

struct Config {
    std::string bridge_ip;
    std::string username;
    std::string clientkey;
    std::string location;
    int light_id;
    std::string owm_api_key;
};

Config loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Failed to open config file");

    json j;
    file >> j;
    return Config{j["bridge_ip"], j["username"], j["clientkey"], j["location"], j["light_id"], j["owm_api_key"]};
}

using namespace std::chrono_literals;

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    if (signal == SIGINT) {
        g_running = false;
    }
}

void testAudioReactiveEntertainment(hueplusplus::Bridge& bridge) {
    std::signal(SIGINT, signalHandler);

    try {
        auto allGroups = bridge.groups().getAll();

        std::string targetName = "Music area";
        auto it = std::ranges::find_if(allGroups, [&targetName](const hueplusplus::Group& g) {
            return g.getName() == targetName;
        });

        if (it == allGroups.end()) {
            std::println("Viihdealuetta '{}' ei löytynyt!", targetName);
            return;
        }

        hueplusplus::Group musicArea = *it;
        size_t lightCount = musicArea.getLightIds().size();

        std::println("Valmistellaan Entertainment-tila: '{}' (ID: {}, valoja: {} kpl)", 
                     musicArea.getName(), musicArea.getId(), lightCount);

        // Alustetaan PulseAudio
        pa_sample_spec ss;
        ss.format = PA_SAMPLE_S16LE;
        ss.rate = 44100;
        ss.channels = 2;

        // Luetaan kerralla vain 512 näytettä (256 stereo-näyteparia)
        // Tavuina tämä on 512 * 2 tavua = 1024 tavua (kestää vain ~5.8 millisekuntia!)
        constexpr size_t BUFFER_SIZE = 512;

        // 2. Määritetään matala latenssi (Puskuri n. 20 ms)
        pa_buffer_attr attr;
        attr.maxlength = BUFFER_SIZE * sizeof(int16_t) * 2; // Maksimissaan vain 2 puskurillista! static_cast<uint32_t>(-1); // Oletus
        attr.tlength   = static_cast<uint32_t>(-1); // Vain toistolle
        attr.prebuf    = static_cast<uint32_t>(-1); // Vain toistolle
        attr.minreq    = static_cast<uint32_t>(-1); // Vain toistolle
        // 44100 Hz * 2 kanavaa * 2 tavua * 0.02 sekuntia = ~3528 tavua puskuriin
        // attr.fragsize  = 44100 * 2 * sizeof(int16_t) * 0.02;
        attr.fragsize  = BUFFER_SIZE * sizeof(int16_t); // Asetetaan fragsize täsmälleen puskurin kokoiseksi!

        int error = 0;
        pa_simple* paStream = pa_simple_new(
            nullptr,                
            "HueAudioVisualizer",   
            PA_STREAM_RECORD,       
            "@DEFAULT_SINK@.monitor",
            "Musiikin kaappaus Huelle",
            &ss,                    
            nullptr,                // Oletus-map
            &attr,                  // <-- Viiveen korjaus (Buffer Attributes)
            &error
        );

        if (!paStream) {
            std::println("Virhe PulseAudio-yhteydessä: {}", pa_strerror(error));
            return;
        }

        // Alustetaan Hue Entertainment
        hueplusplus::EntertainmentMode entertainment(bridge, musicArea);
        std::println("Yhdistetään Hue-suoratoistoon...");
        if (!entertainment.connect()) {
            std::println("Virhe: Hue-suoratoistoyhteyden avaaminen epäonnistui!");
            pa_simple_free(paStream);
            return;
        }

        // --- Alustetaan SFML Oskilloskooppi-ikkuna ---
        constexpr unsigned int WIN_WIDTH = 800;
        constexpr unsigned int WIN_HEIGHT = 400;
        sf::RenderWindow window(sf::VideoMode(WIN_WIDTH, WIN_HEIGHT), "Hue Audio Oscilloscope");
        
        window.setVerticalSyncEnabled(false);
        window.setFramerateLimit(0);

        std::println("ÄÄNIREAKTIO JA OSKILLOSKOOPPI AKTIIVINEN! Sulje ikkuna tai paina Ctrl + C lopettaaksesi.");

        // constexpr size_t BUFFER_SIZE = 1024; // Mono-näytteitä stereo-interleaved -puskurissa
        std::vector<int16_t> audioBuffer(BUFFER_SIZE);

        uint8_t baseColorHue = 0;

        // Peak/waveform-puskuri piirtoa varten (otetaan stereo-puskurista vain vasen kanava 512 näytettä)
        // sf::VertexArray waveform(sf::PrimitiveType::LineStrip, BUFFER_SIZE / 2);

        // Kanavia on 2 (stereo), joten pistemäärä on BUFFER_SIZE / 2
        sf::VertexArray waveform(sf::PrimitiveType::LineStrip, BUFFER_SIZE / 2);

        while (g_running && window.isOpen()) {
            // Tapahtumien käsittely ikkunalle (esim. sulkeminen ruksista)
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    window.close();
                    g_running = false;
                }
            }

            // Tyhjennetään PulseAudion lukujono aina ennen uutta lukua, 
            // jotta saadaan Varmasti REAALIAIKAINEN näyte eikä jonossa odottanutta vanhaa dataa!
            pa_simple_flush(paStream, &error);

            // Luetaan PulseAudion puskuri tyhjäksi siten, että käsiin jää VAIN tuorein data.
            // Tämä estää 10 sekunnin puskurijonon muodostumisen kokonaan.
            bool hasNewData = false;

            // Luetaan niin kauan kuin puskurissa on dataa saatavilla (tai vähintään yksi puskurillinen)
            while (true) {
                // Luetaan puskurillinen
                int readRes = pa_simple_read(paStream, audioBuffer.data(), audioBuffer.size() * sizeof(int16_t), &error);
                if (readRes < 0) {
                    break;
                }
                hasNewData = true;

                // Tarkistetaan, onko PulseAudion puskurissa vielä lisää dataa odottamassa.
                // Jos ei ole (eli ollaan reaaliajassa), poistutaan lukusilmukasta ja piirretään kuva!
                if (pa_simple_get_latency(paStream, &error) < 20000) { // alle 20ms latenssi jäljellä
                    break;
                }
            }


            // Luetaan ääni puskuriin
            // if (pa_simple_read(paStream, audioBuffer.data(), audioBuffer.size() * sizeof(int16_t), &error) < 0) {
            //     break;
            // }

            // Lasketaan RMS-äänenvoimakkuus
            double sumSquare = 0.0;
            for (int16_t sample : audioBuffer) {
                double normalized = sample / 32768.0;
                sumSquare += normalized * normalized;
            }
            float rmsVolume = std::sqrt(sumSquare / audioBuffer.size());

            // --- NOISE GATE ---
            constexpr float NOISE_THRESHOLD = 0.02f;

            if (rmsVolume < NOISE_THRESHOLD) {
                for (uint8_t i = 0; i < static_cast<uint8_t>(lightCount); ++i) {
                    entertainment.setColorRGB(i, 0, 0, 0);
                }
            } else {
                float intensity = std::clamp((rmsVolume - NOISE_THRESHOLD) * 4.0f, 0.0f, 1.0f);

                for (uint8_t i = 0; i < static_cast<uint8_t>(lightCount); ++i) {
                    uint8_t red = 0, green = 0, blue = 0;

                    if (intensity > 0.5f) {
                        red   = static_cast<uint8_t>(255 * intensity);
                        green = static_cast<uint8_t>(50 * intensity);
                        blue  = 0;
                    } else {
                        red   = static_cast<uint8_t>(40 * intensity);
                        green = static_cast<uint8_t>((baseColorHue + i * 25) % 256);
                        blue  = static_cast<uint8_t>(255 * intensity);
                    }

                    entertainment.setColorRGB(i, red, green, blue);
                }
                baseColorHue = (baseColorHue + 1) % 256;
            }

            // Lähetetään paketti Huelle
            entertainment.update();

            // --- OSKILLOSKOOPIN PIIRTÄMINEN ---
            window.clear(sf::Color(15, 15, 20)); // Tumma tausta

            // Keskiviiva (nollataso)
            sf::Vertex middleLine[] = {
                sf::Vertex(sf::Vector2f(0.f, WIN_HEIGHT / 2.f), sf::Color(40, 40, 50)),
                sf::Vertex(sf::Vector2f(static_cast<float>(WIN_WIDTH), WIN_HEIGHT / 2.f), sf::Color(40, 40, 50))
            };
            window.draw(middleLine, 2, sf::PrimitiveType::Lines);

            // Muodostetaan aaltomuoto (käytetään vasenta kanavaa = joka toinen näyte)
            size_t points = BUFFER_SIZE / 2;
            float xStep = static_cast<float>(WIN_WIDTH) / static_cast<float>(points - 1);
            float centerY = WIN_HEIGHT / 2.0f;
            float amplitudeScale = WIN_HEIGHT / 2.0f; // Täysi skaala tilaa varten

            // Vaihdetaan viivan väriä musiikin voimakkuuden/iskun mukaan
            sf::Color lineColor;
            if (rmsVolume > 0.2f) {
                lineColor = sf::Color(255, 80, 40);  // Oranssi/Punainen iskussa
            } else if (rmsVolume > NOISE_THRESHOLD) {
                lineColor = sf::Color(0, 220, 255);  // Syaani perustilassa
            } else {
                lineColor = sf::Color(60, 60, 80);   // Tummansininen hiljaisuudessa
            }

            for (size_t i = 0; i < points; ++i) {
                float sampleNorm = audioBuffer[i * 2] / 32768.0f; // Normalisointi [-1.0, 1.0]
                float x = i * xStep;
                float y = centerY - (sampleNorm * amplitudeScale);

                waveform[i].position = sf::Vector2f(x, y);
                waveform[i].color = lineColor;
            }

            window.draw(waveform);
            window.display();
        }        

        std::println("\nKatkaistaan suoratoisto ja suljetaan äänikaappaus...");
        entertainment.disconnect();
        if (paStream) {
            pa_simple_free(paStream);
        }
        if (window.isOpen()) {
            window.close();
        }
        std::println("Sammutettu hallitusti.");

    } catch (const std::exception& e) {
        std::println("Virhe: {}", e.what());
    }
}

int main() {
    try {
        Config cfg = loadConfig("config.json");

        auto handler = std::make_shared<CurlHttpHandler>();

        int port = 443;
        Bridge bridge(cfg.bridge_ip, port, cfg.username, handler, cfg.clientkey);

        testAudioReactiveEntertainment(bridge);
        return 0;

    } catch(const std::exception& e) {
        std::print(stderr, "Error: {}\n", e.what());
        return 1;
    }
}