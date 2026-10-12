#!/usr/bin/env python3
import os
import shutil
import datetime
from pathlib import Path
import re

def get_kicad_config_dir():
    """Najde složku s konfigurací pro nejnovější nainstalovanou verzi KiCadu."""
    base_dir = Path.home() / ".config" / "kicad"
    if not base_dir.exists():
        return None
    
    versions = [d.name for d in base_dir.iterdir() if d.is_dir() and re.match(r'^\d+\.\d+$', d.name)]
    if not versions:
        return None
    
    latest_version = sorted(versions, key=lambda x: float(x))[-1]
    return base_dir / latest_version

def backup_file(filepath):
    """Vytvoří zálohu souboru s časovým razítkem, pokud soubor existuje."""
    if not filepath.exists():
        return False
    
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_path = filepath.with_name(f"{filepath.name}.backup_{timestamp}")
    shutil.copy2(filepath, backup_path)
    print(f"[ZÁLOHA] Vytvořena záloha: {backup_path}")
    return True

def add_to_table(table_path, libs, lib_type="KiCad"):
    """Přidá nové knihovny do S-expression tabulky KiCadu."""
    if not libs:
        return

    if not table_path.exists():
        print(f"[CHYBA] Soubor {table_path.name} neexistuje. Otevřete nejprve KiCad, aby se vytvořil.")
        return

    with open(table_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        content = "".join(lines)

    # Zjistíme existující názvy a cesty pro kontrolu duplicit
    existing_names = set(re.findall(r'\(name\s+"([^"]+)"\)', content))
    existing_uris = set(re.findall(r'\(uri\s+"([^"]+)"\)', content))

    new_entries = []
    for name, path in libs.items():
        if name in existing_names:
            print(f"[PŘESKOČENO] Knihovna s názvem '{name}' už je v KiCadu importovaná.")
        elif path in existing_uris:
            print(f"[PŘESKOČENO] Cesta '{path}' už v KiCadu existuje pod jiným jménem.")
        else:
            entry = f'    (lib (name "{name}")(type "{lib_type}")(uri "{path}")(options "")(descr ""))\n'
            new_entries.append(entry)
            print(f"[K PŘIDÁNÍ] {name} -> {table_path.name}")

    # Pokud není co nového přidat, rovnou skončíme
    if not new_entries:
        print(f"[INFO] Do {table_path.name} nebylo potřeba přidávat nic nového.")
        return

    # Provedeme zálohu těsně před zápisem nových dat
    backup_file(table_path)

    # Najdeme poslední uzavírací závorku a vložíme nové záznamy před ni
    for i in reversed(range(len(lines))):
        if ')' in lines[i]:
            lines.insert(i, "".join(new_entries))
            break

    # Zapíšeme upravená data zpět do souboru
    with open(table_path, 'w', encoding='utf-8') as f:
        f.writelines(lines)
        print(f"[ÚSPĚCH] Úspěšně přidáno {len(new_entries)} knihoven do {table_path.name}.\n")

def main():
    current_dir = Path.cwd()
    kicad_dir = get_kicad_config_dir()
    
    print("=" * 50)
    print("KiCad Library Auto-Importer")
    print("=" * 50)
    print(f"Skenuji složku (včetně podsložek): {current_dir}")
    
    if not kicad_dir:
        print("[CHYBA] Nenalezena konfigurace KiCadu v ~/.config/kicad/")
        return

    print(f"Cílová konfigurace KiCadu: {kicad_dir}\n")

    # Hledání schématických značek (.kicad_sym) pomocí rglob (rekurzivně)
    sym_libs = {}
    for sym_file in current_dir.rglob("*.kicad_sym"):
        sym_libs[sym_file.stem] = str(sym_file.absolute())

    # Hledání footprintů (složky .pretty) pomocí rglob (rekurzivně)
    fp_libs = {}
    for fp_dir in current_dir.rglob("*.pretty"):
        fp_libs[fp_dir.stem] = str(fp_dir.absolute())

    print(f"Nalezeno {len(sym_libs)} schématických knihoven a {len(fp_libs)} footprint knihoven ke kontrole.\n")

    # Zpracování schématických značek
    print("--- Zpracovávám Symboly (sym-lib-table) ---")
    if sym_libs:
        add_to_table(kicad_dir / "sym-lib-table", sym_libs)
    else:
        print("[INFO] Žádné .kicad_sym knihovny nenalezeny.\n")

    # Zpracování footprintů
    print("--- Zpracovávám Footprinty (fp-lib-table) ---")
    if fp_libs:
        add_to_table(kicad_dir / "fp-lib-table", fp_libs)
    else:
        print("[INFO] Žádné .pretty knihovny nenalezeny.\n")
        
    print("Hotovo! Můžete spustit/restartovat KiCad.")

if __name__ == "__main__":
    main()
