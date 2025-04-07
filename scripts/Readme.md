**Ferramentas BLE**

Este repositório contém um conjunto de scripts Python para gerenciamento e interação com dispositivos e serviços BLE. Cada ferramenta possui um papel específico:

1. **scan_ble.py** – Scanner BLE com opção de filtro por MAC e exibição detalhada dos dados de advertisement.
2. **create_service.py** – Gera arquivos de serviço BLE (header e source) a partir de um arquivo JSON de descrição.
3. **set_mac.py** – Gerencia a lista de dispositivos permitidos (accept list) via BLE, permitindo adicionar, remover ou limpar dispositivos.
4. **shadow_client.py** – Cliente BLE que se conecta a um dispositivo do tipo "Worker Shadow Service" para receber e interpretar notificações de atualização de estado.

**scan_ble.py**

Este script realiza a varredura de dispositivos BLE utilizando a biblioteca [Bleak](https://github.com/hbldh/bleak). Ele possui as seguintes funcionalidades:

- **Filtro por MAC:** Se um arquivo contendo MACs for especificado, apenas os dispositivos cujos endereços constem na lista serão processados.
- **Exibição de detalhes:** Mostra informações do dispositivo, dados do fabricante e, se o parâmetro -sensor for informado, exibe detalhes dos dados do sensor (tipo, timestamp, valores, etc.).
- **Saída em arquivo:** Permite salvar a saída do scan em um arquivo.

**Opções de Parâmetros**
scan_ble.py \[-h\] \[-m mac_file.txt\] \[-t time\] \[-o output.txt\] \[-sensor\]

- **\-m**: Caminho para o arquivo contendo os endereços MAC a serem filtrados.
- **\-t**: Tempo de escaneamento em segundos. Se não informado, o scanner executa indefinidamente até ser interrompido.
- **\-o**: Arquivo para salvar a saída do scan.
- **\-sensor**: Flag para exibir detalhes dos dados do sensor.

**Exemplo de Uso**

```bash
python scan_ble.py -t 30 -m mac_filter.txt -o scan_output.txt -sensor
```
___________________________________________________
**create_service.py**

Este script gera arquivos de serviço BLE (cabeçalho e fonte) a partir de um arquivo JSON que descreve o serviço. O JSON deve conter a definição do serviço, incluindo nome, UUID e características (com seus respectivos UUIDs e propriedades).

**Opções de Parâmetros**

create_service.py -json JSON_FILE \[--generate-uuid\]

- **\-json**: Caminho para o arquivo JSON com a descrição do serviço.
- **\--generate-uuid**: Flag opcional. Se informado, gera novos UUIDs aleatórios para o serviço e suas características, ignorando os valores do JSON.

**Exemplo de Uso**

```bash
python create_service.py -json ble_sensor_service.json --generate-uuid
```

Após a execução, serão gerados dois arquivos:

- Um arquivo de cabeçalho (&lt;nome_do_serviço&gt;.h)
- Um arquivo fonte (&lt;nome_do_serviço&gt;.c)

________________________________________________________

**set_mac.py**

Esta ferramenta gerencia a lista de dispositivos permitidos (accept list) de um concentrador BLE. Ela possibilita adicionar, remover ou limpar a lista, enviando comandos via BLE para características específicas do serviço de filtro.

**Opções de Parâmetros**

set_mac.py (--add | --rem | --clear) \[--mac_list MAC_LIST\] \[--mac MAC\] \[--type MAC_TYPE\]

- **\--add**: Comando para adicionar MACs à lista.
- **\--rem**: Comando para remover MACs da lista.
- **\--clear**: Comando para limpar toda a lista.
- **\--mac_list**: Arquivo com a lista de MACs (um por linha, com tipo opcional).
- **\--mac**: Um único endereço MAC no formato AA:BB:CC:DD:EE:FF.
- **\--type**: Tipo do MAC em hexadecimal (padrão: 0x01). Utilizado quando se fornece um único MAC com a opção --mac.

**Atenção:** Para os comandos de adicionar ou remover, deve-se especificar **apenas uma** das opções: --mac_list ou --mac.

**Exemplos de Uso**

Adicionar MACs a partir de um arquivo:

```bash
python set_mac.py --add --mac_list mac_filter.txt
```

Remover um único MAC:

```bash
python set_mac.py --rem --mac AA:BB:CC:DD:EE:FF --type 0x01
```

Limpar a lista de dispositivos:

```bash
python set_mac.py --clear
```
_____________________________________________________________________
**shadow_client.py**

Este script atua como cliente BLE para o "Worker Shadow Service". Ele:

- Realiza a varredura de dispositivos procurando por um serviço com um UUID específico.
- Conecta-se ao dispositivo encontrado e se inscreve para receber notificações do caractere "Worker Shadow".
- Recebe os dados de shadow e os interpreta, extraindo informações como timestamp, temperatura, pressão, latitude, longitude, tipo de fixação, movimento, postura e intensidade de luz.

**Uso**

O script não possui parâmetros de linha de comando; basta executá-lo:

bash

Copy

python shadow_client.py

Ele ficará em execução aguardando notificações até que seja interrompido (Ctrl+C).
