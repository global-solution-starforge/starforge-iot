StarForge — Painel de Monitoramento CubeSat
Equipe

Anna Clara Russo Luca — RM: 561928
Gabriel Duarte Maciel — RM: 565754
Tiago Guedes da Costa — RM: 564731
Gustavo Tavares — RM: 562827


O Problema
O espaço sempre foi caro, distante e pouco acessível para o público geral. Programas espaciais são acompanhados por especialistas, nunca pelo cidadão comum. Não existe uma forma interativa, engajante e real de participar de uma missão espacial — e é exatamente esse vácuo que o StarForge ocupa.

Solução: StarForge
StarForge é uma plataforma de crowdfunding espacial onde jogadores financiam missões reais de CubeSats brasileiros e recebem recompensas em um jogo quando a meta coletiva é atingida. O protótipo IoT é o painel físico de monitoramento dessa missão — uma estação de controle em miniatura que representa em tempo real o progresso do financiamento, a telemetria simulada do satélite e o status de cada missão.
O diferencial central é a experiência bidirecional: o painel físico e o dashboard web operam em sincronia total via MQTT. Uma contribuição feita pelo botão físico aparece instantaneamente no dashboard, e uma contribuição feita pelo dashboard reflete imediatamente no display OLED e nos LEDs.

Por que IoT?

Experiência tangível — LEDs, buzzer e display OLED tornam o progresso de uma missão espacial algo físico e presente, não apenas uma barra de progresso numa tela.
Tempo real — telemetria simulada do satélite publicada via MQTT a cada 3 segundos, refletida simultaneamente no painel físico e no dashboard.
Bidirecionalidade — o sistema não apenas exibe dados, ele recebe comandos. Dashboard e hardware físico controlam o mesmo estado de missão.
Escalabilidade — três missões independentes gerenciadas pelo mesmo firmware, cada uma com progresso, ranking e estado próprios.


Arquitetura
┌─────────────────────────────────────────┐
│ ESP32 DevKit V1 (Wokwi)                 │
│  ├─ Potenciômetro (GPIO 34) → contribuição │
│  ├─ Botão push    (GPIO 14) → confirmar    │
│  ├─ LED Vermelho  (GPIO 25) → abaixo 50%   │
│  ├─ LED Amarelo   (GPIO 26) → 50% a 99%    │
│  ├─ LED Verde     (GPIO 27) → meta atingida│
│  ├─ Buzzer        (GPIO 32) → feedback sonoro│
│  └─ OLED SSD1306  (I²C 21/22) → interface local│
└─────────────────────────────────────────┘
                ↓ MQTT (TCP 1883)
┌─────────────────────────────────────────┐
│ Broker EMQX público                     │
│   broker.emqx.io                        │
└─────────────────────────────────────────┘
                ↓ MQTT (WS 8083)
┌─────────────────────────────────────────┐
│ Dashboard HTML (navegador, file://)     │
│  3 missões · ranking · telemetria       │
└─────────────────────────────────────────┘

Funcionalidades Implementadas
✅ Firmware ESP32 com leitura de potenciômetro e botão com debounce
✅ Display OLED exibindo nome da missão, barra de progresso, status e temperatura
✅ 3 LEDs indicando status por cor — vermelho, amarelo e verde piscando
✅ Buzzer com beep de confirmação e melodia de celebração ao desbloquear missão
✅ 3 missões independentes com progresso, estado e ranking próprios
✅ Máquina de estados: Aguardando → Financiando → Em Órbita
✅ Avanço automático para a próxima missão ao atingir 100%
✅ Ranking top 5 contribuidores por missão com acumulação por nome
✅ MQTT bidirecional — publica telemetria e recebe comandos do dashboard
✅ API REST com endpoint /status retornando JSON completo
✅ Dashboard HTML sem servidor, conectado via WebSocket ao broker MQTT
✅ Reconexão automática Wi-Fi e MQTT

Tópicos MQTT Documentados
Publicados pelo ESP32
TópicoConteúdostarforge/mission/telemetriaJSON completo da missão ativa a cada 3sstarforge/mission/progressPercentual de financiamento (0–100)starforge/mission/statusEstado textual da missãostarforge/mission/rankingJSON com top 5 contribuidoresstarforge/mission/unlockedEvento de missão desbloqueadastarforge/mission/listLista completa das 3 missões
Assinados pelo ESP32 (comandos do dashboard)
TópicoPayloadstarforge/cmd/contribute{"nome":"João","valor":10}starforge/cmd/select{"idx":1}starforge/cmd/reset{"confirm":true}

Stack Técnico
TecnologiaFinalidadeESP32 DevKit V1Microcontrolador principal (simulado no Wokwi)Adafruit SSD1306Driver do display OLEDAdafruit GFXBiblioteca gráfica para o OLEDPubSubClientCliente MQTT no firmwareArduinoJsonSerialização e desserialização JSONbroker.emqx.ioBroker MQTT público (portas 1883 / 8083)Paho MQTT JSCliente MQTT no navegador via WebSocketHTML / CSS / JSDashboard frontend sem build step

Como Executar
A. Rodar o firmware no Wokwi

Acesse wokwi.com e crie um novo projeto ESP32
Copie o conteúdo de sketch.ino para a aba sketch.ino
Copie o conteúdo de diagram.json para a aba diagram.json
No Library Manager do Wokwi adicione:

Adafruit GFX Library
Adafruit SSD1306
PubSubClient
ArduinoJson


Clique em Play ▶

Saída esperada no Serial Monitor:
Wi-Fi conectado! IP: 10.10.0.2
Conectando ao MQTT... OK
Assinando topicos de comando...
Servidor HTTP iniciado na porta 80
Missao ativa: CubeSat BR-01
B. Abrir o dashboard

Baixe o arquivo dashboard.html
Abra direto no navegador
O dashboard conecta automaticamente ao broker e começa a receber os dados em segundos

Missões
MissãoDescriçãoPrazoCubeSat BR-01Monitoramento climático2025-06-30CubeSat BR-02Imageamento orbital2025-09-30CubeSat BR-03Comunicação de emergência2025-12-31

Lógica de Status dos LEDs
ProgressoLED0% a 49%Vermelho fixo50% a 99%Amarelo fixo100%Verde piscando

Requisitos Atendidos
RequisitoStatusProtótipo funcional ESP32 no Wokwi✅2 entradas (potenciômetro + botão)✅2 saídas (LEDs + buzzer)✅Interface local OLED SSD1306✅Comunicação Wi-Fi✅WebServer HTTP + API REST✅Mínimo 3 endpoints/tópicos MQTT documentados✅ 9 tópicosDashboard para visualização dos dados✅