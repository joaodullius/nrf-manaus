#!/usr/bin/env python3
import argparse
import json
import os
import re
import uuid

def to_macro(name):
    # Converte para maiúsculas e substitui caracteres não alfanuméricos por underscore.
    return re.sub(r'\W+', '_', name.upper())

def to_filename(name):
    # Converte para minúsculas e substitui caracteres não alfanuméricos por underscore.
    return re.sub(r'\W+', '_', name.lower())

def uuid_to_encode_args(uuid_str):
    """Converte uma string UUID 'xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx' para os argumentos da macro BT_UUID_128_ENCODE."""
    parts = uuid_str.split('-')
    if len(parts) != 5:
        raise ValueError("Formato de UUID inválido: " + uuid_str)
    return f"0x{parts[0]}, 0x{parts[1]}, 0x{parts[2]}, 0x{parts[3]}, 0x{parts[4]}"

def get_perm(perm_type, props):
    # perm_type é 'read' ou 'write'
    sec = props.get("security", {})
    if perm_type in sec:
        if sec[perm_type] == "encrypt":
            return f"BT_GATT_PERM_{perm_type.upper()}_ENCRYPT"
        elif sec[perm_type] == "authen":
            return f"BT_GATT_PERM_{perm_type.upper()}_AUTHEN"
    # Se não houver segurança, ou for "none", retorna a permissão padrão.
    return f"BT_GATT_PERM_{perm_type.upper()}"

def gen_header(service):
    service_name = service["name"]
    service_macro = to_macro(service_name)
    filename_base = to_filename(service_name)
    service_uuid = service["UUID"]
    service_uuid_args = uuid_to_encode_args(service_uuid)
    
    header = []
    header.append(f"#ifndef {service_macro}_H")
    header.append(f"#define {service_macro}_H\n")
    header.append("#include <zephyr/bluetooth/bluetooth.h>\n")
    
    header.append(f"// Service UUID")
    header.append(f"#define {service_macro}_UUID BT_UUID_128_ENCODE({service_uuid_args})\n")
    
    # Gera defines de UUID para cada característica
    for char in service["characteristics"]:
        char_macro = to_macro(char["name"]) + "_CHAR"
        header.append(f"// {char['name']} characteristic")
        header.append(f"#define {char_macro}_UUID BT_UUID_128_ENCODE({uuid_to_encode_args(char['UUID'])})\n")
    
    # Protótipo para a função de inicialização
    header.append(f"int {to_filename(service_name)}_init(void);")
    
    # Para cada característica com notify, adiciona protótipo para função de notificação.
    for char in service["characteristics"]:
        props = char.get("properties", {})
        if props.get("notify", False):
            char_name = to_filename(char["name"])
            ctype = props.get("type", "uint8_t")
            header.append(f"int send_{char_name}_notification({ctype} {char_name});")
    
    header.append(f"\n#endif /* {service_macro}_H */")
    return "\n".join(header)

def gen_callback_stub(char, cb_type):
    # cb_type é "read" ou "write"
    name = char["name"]
    func_name = f"on_{to_filename(name)}_{cb_type}"
    var_name = f"{to_filename(name)}_var"
    if cb_type == "read":
        stub = f"""static ssize_t {func_name}(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    void *buf, uint16_t len, uint16_t offset) {{
    // TODO: Implementar callback de leitura para "{name}"
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &{var_name}, sizeof({var_name}));
}}"""
    elif cb_type == "write":
        stub = f"""static ssize_t {func_name}(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {{
    // TODO: Implementar callback de escrita para "{name}"
    return len;
}}"""
    else:
        stub = ""
    return stub

def gen_source(service):
    service_name = service["name"]
    filename_base = to_filename(service_name)
    service_macro = to_macro(service_name)
    source = []
    source.append(f'#include "{filename_base}.h"')
    source.append("#include <zephyr/bluetooth/uuid.h>")
    source.append("#include <zephyr/bluetooth/gatt.h>")
    source.append("#include <zephyr/logging/log.h>\n")
    source.append(f"LOG_MODULE_REGISTER({filename_base}, LOG_LEVEL_INF);\n")
    
    # Declaração de variáveis globais para características de leitura e flags para notificações.
    for char in service["characteristics"]:
        props = char.get("properties", {})
        if props.get("read", False):
            var_name = f"{to_filename(char['name'])}_var"
            ctype = props.get("type", "uint8_t")
            source.append(f"// Variável global para a característica de leitura \"{char['name']}\".")
            source.append(f"static {ctype} {var_name};\n")
        if props.get("notify", False):
            source.append(f"// Flag global para notificações de \"{char['name']}\".")
            source.append(f"bool {to_filename(char['name'])}_notify_enabled;\n")
            source.append(f"""static void {to_filename(char['name'])}_ccc_change(const struct bt_gatt_attr *attr, uint16_t value)
{{
    {to_filename(char['name'])}_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("{char['name']} notifications %s", {to_filename(char['name'])}_notify_enabled ? "enabled" : "disabled");
}}\n""")
    
    # Gera os callbacks para cada característica.
    callback_names = {}
    for char in service["characteristics"]:
        props = char.get("properties", {})
        stubs = []
        if props.get("read", False):
            stub = gen_callback_stub(char, "read")
            stubs.append(stub)
            callback_names[char["name"] + "_read"] = f"on_{to_filename(char['name'])}_read"
        if props.get("write", False):
            stub = gen_callback_stub(char, "write")
            stubs.append(stub)
            callback_names[char["name"] + "_write"] = f"on_{to_filename(char['name'])}_write"
        if stubs:
            source.append("\n".join(stubs) + "\n")
    
    # Geração do bloco BT_GATT_SERVICE_DEFINE.
    attr_index = 1  # índice de atributo (depois do serviço primário)
    notify_info = []  # para armazenar informações necessárias para notificações
    
    source.append("BT_GATT_SERVICE_DEFINE({0}_svc,".format(filename_base))
    source.append("    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128({0}_UUID)),"
                  .format(service_macro))
    
    for char in service["characteristics"]:
        char_macro = to_macro(char["name"]) + "_CHAR"
        props = char.get("properties", {})
        # Define as propriedades da característica.
        prop_list = []
        if props.get("read", False):
            prop_list.append("BT_GATT_CHRC_READ")
        if props.get("write", False):
            prop_list.append("BT_GATT_CHRC_WRITE")
        if props.get("notify", False):
            prop_list.append("BT_GATT_CHRC_NOTIFY")
        if props.get("indicate", False):
            prop_list.append("BT_GATT_CHRC_INDICATE")
        prop_str = " | ".join(prop_list) if prop_list else "0"
        
        # Define as permissões, levando em conta o objeto "security" se presente.
        perm_list = []
        if props.get("read", False):
            perm_list.append(get_perm("read", props))
        if props.get("write", False):
            perm_list.append(get_perm("write", props))
        perm_str = " | ".join(perm_list) if perm_list else "0"
        
        # Determina os callbacks.
        read_cb = callback_names.get(char["name"] + "_read", "NULL")
        write_cb = callback_names.get(char["name"] + "_write", "NULL")
        value_ptr = f"&{to_filename(char['name'])}_var" if props.get("read", False) else "NULL"
        
        source.append("    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128({0}_UUID),".format(char_macro))
        source.append("                           {0},".format(prop_str))
        source.append("                           {0},".format(perm_str))
        source.append("                           {0}, {1}, {2}),".format(read_cb, write_cb, value_ptr))
        value_attr_index = attr_index + 1
        attr_index += 2
        # Se notificações estiverem habilitadas, adiciona o CCC com permissões de segurança configuradas.
        if props.get("notify", False):
            ccc_perms = "BT_GATT_PERM_READ | BT_GATT_PERM_WRITE"
            sec = props.get("security", {})
            if "notify" in sec:
                if sec["notify"] == "encrypt":
                    ccc_perms = "BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT"
                elif sec["notify"] == "authen":
                    ccc_perms = "BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN"
            source.append("    BT_GATT_CCC({0}_ccc_change, {1}),".format(to_filename(char["name"]), ccc_perms))
            notify_info.append((to_filename(char["name"]), props.get("type", "uint8_t"), value_attr_index))
            attr_index += 1
    if source[-1].endswith(","):
        source[-1] = source[-1].rstrip(",")
    source.append(");")
    
    # Gera funções para envio de notificação para características habilitadas para notify.
    for info in notify_info:
        char_name, ctype, value_index = info
        source.append(f"""
/* Envia notificação para a característica {char_name} */
int send_{char_name}_notification({ctype} {char_name})
{{
    {char_name}_var = {char_name};
    if (!{char_name}_notify_enabled) {{
        return -EACCES;
    }}
    /* O atributo de valor está assumido no índice {value_index} na estrutura do serviço. */
    return bt_gatt_notify(NULL, &{filename_base}_svc.attrs[{value_index}],
                            &{char_name}_var, sizeof({char_name}_var));
}}
""")
    
    source.append(f"\nint {filename_base}_init(void) {{")
    source.append(f"    LOG_INF(\"{service_name} service initialized\");")
    source.append("    return 0;")
    source.append("}")
    
    return "\n".join(source)

def main():
    parser = argparse.ArgumentParser(description="Gera arquivos para serviço BLE customizado a partir de JSON")
    parser.add_argument("-json", required=True, help="Arquivo JSON com a descrição do serviço")
    parser.add_argument("--generate-uuid", action="store_true", help="Gera novos UUIDs aleatórios, ignorando os do JSON")
    args = parser.parse_args()

    with open(args.json, "r") as f:
        data = json.load(f)
    
    service = data.get("service")
    if not service:
        print("O JSON não contém a chave 'service'.")
        return

    if args.generate_uuid:
        service["UUID"] = str(uuid.uuid4())
        for char in service["characteristics"]:
            char["UUID"] = str(uuid.uuid4())
    
    header_content = gen_header(service)
    source_content = gen_source(service)
    
    filename_base = to_filename(service["name"])
    header_filename = f"{filename_base}.h"
    source_filename = f"{filename_base}.c"

    with open(header_filename, "w") as f:
        f.write(header_content)
    with open(source_filename, "w") as f:
        f.write(source_content)
    
    print(f"Gerado {header_filename} e {source_filename}")

if __name__ == "__main__":
    main()
