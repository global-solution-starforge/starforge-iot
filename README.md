## 👥 Equipe
Link do vídeo: https://www.youtube.com/watch?v=BZOQ5KRoEqY
| Nome | RM |
|---|---|
| Anna Clara Russo Luca | 561928 |
| Gabriel Duarte Maciel | 565754 |
| Tiago Guedes da Costa | 564731 |
| Gustavo Tavares | 562827 |

---

## 🚀 O Problema

Programas espaciais são caros, distantes e pouco acessíveis para o público geral. Não existe uma forma interativa, engajante e real de participar de uma missão espacial. É exatamente esse vácuo que o StarForge ocupa.

---

## 💡 A Solução

O StarForge é uma plataforma de crowdfunding espacial onde jogadores financiam missões reais de CubeSats brasileiros e recebem recompensas em um jogo quando a meta coletiva é atingida.

O protótipo IoT é o **painel físico de monitoramento** dessa missão — uma estação de controle em miniatura que representa em tempo real o progresso do financiamento, a telemetria simulada do satélite e o status de cada missão.

O diferencial central é a **experiência bidirecional**: o painel físico e o dashboard web operam em sincronia total via MQTT. Uma contribuição feita pelo botão físico aparece instantaneamente no dashboard, e uma contribuição feita pelo dashboard reflete imediatamente no display OLED e nos LEDs.

---

## 🌐 Por que IoT?

- **Experiência tangível** — LEDs, buzzer e display OLED tornam o progresso de uma missão espacial algo físico e presente
- **Tempo real** — telemetria simulada publicada via MQTT a cada 3 segundos, refletida simultaneamente no painel físico e no dashboard
- **Bidirecionalidade** — o sistema não apenas exibe dados, ele recebe comandos
- **Escalabilidade** — três missões independentes gerenciadas pelo mesmo firmware

---

## 🏗️ Arquitetura

```
┌─────────────────────────────────────────────┐
│ ESP32 DevKit V1 (Wokwi)                     │
│  ├─ Potenciômetro (GPIO 34) → contribuição  │
│  ├─ Botão push    (GPIO 14) → confirmar     │
│  ├─ LED Vermelho  (GPIO 25) → abaixo 50%    │
│  ├─ LED Amarelo   (GPIO 26) → 50% a 99%     │
│  ├─ LED Verde     (GPIO 27) → meta atingida │
│  ├─ Buzzer        (GPIO 32) → feedback      │
│  └─ OLED SSD1306  (I²C 21/22) → interface  │
└─────────────────────────────────────────────┘
                  ↓ MQTT (TCP 1883)
┌─────────────────────────────────────────────┐
│ Broker EMQX público — broker.emqx.io        │
└─────────────────────────────────────────────┘
                  ↓ MQTT (WS 8083)
┌─────────────────────────────────────────────┐
│ Dashboard HTML (navegador, file://)         │
│ 3 missões · ranking · telemetria ao vivo    │
└─────────────────────────────────────────────┘
```

---

## ✅ Funcionalidades Implementadas

- [x] Firmware ESP32 com leitura de potenciômetro e botão com debounce
- [x] Display OLED exibindo nome da missão, barra de progresso, status e temperatura
- [x] 3 LEDs indicando status por cor — vermelho, amarelo e verde piscando
- [x] Buzzer com beep de confirmação e melodia de celebração ao desbloquear missão
- [x] 3 missões independentes com progresso, estado e ranking próprios
- [x] Máquina de estados: Aguardando → Financiando → Em Órbita
- [x] Avanço automático para a próxima missão ao atingir 100%
- [x] Ranking top 5 contribuidores por missão com acumulação por nome
- [x] MQTT bidirecional — publica telemetria e recebe comandos do dashboard
- [x] API REST com endpoint `/status` retornando JSON completo
- [x] Dashboard HTML sem servidor, conectado via WebSocket ao broker MQTT
- [x] Reconexão automática Wi-Fi e MQTT

---

## 📡 Tópicos MQTT Documentados

### Publicados pelo ESP32

| Tópico | Conteúdo |
|---|---|
| `starforge/mission/telemetria` | JSON completo da missão ativa a cada 3s |
| `starforge/mission/progress` | Percentual de financiamento (0–100) |
| `starforge/mission/status` | Estado textual da missão |
| `starforge/mission/ranking` | JSON com top 5 contribuidores |
| `starforge/mission/unlocked` | Evento de missão desbloqueada |
| `starforge/mission/list` | Lista completa das 3 missões |

### Assinados pelo ESP32 (comandos do dashboard)

| Tópico | Payload |
|---|---|
| `starforge/cmd/contribute` | `{"nome":"João","valor":10}` |
| `starforge/cmd/select` | `{"idx":1}` |
| `starforge/cmd/reset` | `{"confirm":true}` |

---

## 🔌 Pinout

| Componente | Pino ESP32 | Observação |
|---|---|---|
| Potenciômetro | GPIO 34 | ADC1 — compatível com Wi-Fi ativo |
| Botão push | GPIO 14 | INPUT_PULLUP — LOW = pressionado |
| LED Vermelho | GPIO 25 | Resistor 220Ω em série |
| LED Amarelo | GPIO 26 | Resistor 220Ω em série |
| LED Verde | GPIO 27 | Resistor 220Ω em série |
| Buzzer | GPIO 32 | Buzzer passivo |
| OLED SDA | GPIO 21 | I2C padrão |
| OLED SCL | GPIO 22 | I2C padrão |

---

## 💡 Lógica de Status dos LEDs

| Progresso | LED |
|---|---|
| 0% a 49% | 🔴 Vermelho fixo |
| 50% a 99% | 🟡 Amarelo fixo |
| 100% | 🟢 Verde piscando |

---

## 🛸 Missões

| Missão | Descrição | Prazo |
|---|---|---|
| CubeSat BR-01 | Monitoramento climático | 2025-06-30 |
| CubeSat BR-02 | Imageamento orbital | 2025-09-30 |
| CubeSat BR-03 | Comunicação de emergência | 2025-12-31 |

---

## 🧰 Stack Técnico

| Tecnologia | Finalidade |
|---|---|
| ESP32 DevKit V1 | Microcontrolador principal (simulado no Wokwi) |
| Adafruit SSD1306 | Driver do display OLED |
| Adafruit GFX | Biblioteca gráfica para o OLED |
| PubSubClient | Cliente MQTT no firmware |
| ArduinoJson | Serialização e desserialização JSON |
| broker.emqx.io | Broker MQTT público (portas 1883 / 8083) |
| Paho MQTT JS | Cliente MQTT no navegador via WebSocket |
| HTML / CSS / JS | Dashboard frontend sem build step |

---

## ▶️ Como Executar

### A. Rodar o firmware no Wokwi

1. Acesse [wokwi.com](https://wokwi.com) e crie um novo projeto ESP32
2. Copie o conteúdo de `sketch.ino` para a aba `sketch.ino`
3. Copie o conteúdo de `diagram.json` para a aba `diagram.json`
4. No **Library Manager** do Wokwi adicione:
   - `Adafruit GFX Library`
   - `Adafruit SSD1306`
   - `PubSubClient`
   - `ArduinoJson`
5. Clique em **Play ▶**

**Saída esperada no Serial Monitor:**
```
Wi-Fi conectado! IP: 10.10.0.2
Conectando ao MQTT... OK
Assinando topicos de comando...
Servidor HTTP iniciado na porta 80
Missao ativa: CubeSat BR-01
```

### B. Abrir o dashboard

1. Baixe o arquivo `dashboard.html`
2. Abra direto no navegador
3. O dashboard conecta automaticamente ao broker e começa a receber os dados em segundos

> Não precisa de servidor, sem `npm install`, sem build.

---

## 📋 Requisitos Atendidos

| Requisito | Status |
|---|---|
| Protótipo funcional ESP32 no Wokwi | ✅ |
| 2 entradas — potenciômetro e botão | ✅ |
| 2 saídas — LEDs e buzzer | ✅ |
| Interface local OLED SSD1306 | ✅ |
| Comunicação Wi-Fi | ✅ |
| WebServer HTTP + API REST | ✅ |
| Mínimo 3 tópicos MQTT documentados | ✅ 9 tópicos |
| Dashboard para visualização dos dados | ✅ |

---

## 📁 Estrutura do Repositório

```
starforge/
├── sketch.ino        # Firmware ESP32
├── diagram.json      # Circuito Wokwi
├── libraries.txt     # Dependências Wokwi
├── dashboard.html    # Dashboard local
└── README.md         # Este arquivo
```

---

*Anna Clara · Gabriel Duarte · Tiago Guedes · Gustavo Tavares — FIAP 2025*
