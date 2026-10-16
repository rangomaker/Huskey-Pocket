#pragma once
// =============================================================
//  vault_csv.h — Exportar/importar contraseñas en CSV
//  Requiere vault.h y sd_manager.h incluidos antes
// =============================================================

// ── CSV: Parser RFC 4180 ──────────────────────────────────────
// Parsea una línea CSV respetando campos entre comillas
static char _csv_sep = ','; // separador detectado automáticamente

static void _csv_detect_sep(const char* header) {
    // Contar tabs vs comas SOLO fuera de comillas: un campo citado que
    // contenga comas o tabs no debe influir en la detección del separador.
    int tabs = 0, commas = 0;
    bool in_quotes = false;
    for (const char* p = header; *p; p++) {
        if (*p == '"') { in_quotes = !in_quotes; continue; }
        if (in_quotes) continue;
        if (*p == '\t') tabs++;
        else if (*p == ',') commas++;
    }
    _csv_sep = (tabs > commas) ? '\t' : ',';
}

// Ancho de campo del parser: el mayor de los campos posibles (notas=128).
// Los límites por tipo de campo se validan aparte (_csv_field_fits / _csv_notes_fits).
#define CSV_FIELD_LEN VAULT_NOTES_LEN

static int _csv_parse_line(const char* line, char fields[][CSV_FIELD_LEN], int max_fields) {
    if (max_fields <= 0) return 0;
    int field = 0;
    int fi = 0;
    bool in_quotes = false;
    const char* p = line;

    while (*p) {
        if (in_quotes) {
            if (*p == '"' && *(p+1) == '"') {
                if (fi < CSV_FIELD_LEN - 1) fields[field][fi++] = '"';
                p++;
            } else if (*p == '"') {
                in_quotes = false;
            } else {
                if (fi < CSV_FIELD_LEN - 1) fields[field][fi++] = *p;
            }
        } else {
            if (*p == '"') {
                in_quotes = true;
            } else if (*p == _csv_sep) {
                fields[field][fi] = '\0';
                // No avanzar más allá del último slot: si ya no caben más
                // campos, se ignoran los separadores restantes (evita escribir
                // en fields[max_fields], fuera del arreglo).
                if (field + 1 >= max_fields) break;
                field++;
                fi = 0;
            } else {
                if (fi < CSV_FIELD_LEN - 1) fields[field][fi++] = *p;
            }
        }
        p++;
    }
    fields[field][fi] = '\0';  // field siempre < max_fields aquí
    return field + 1;
}

// Un campo que exceda VAULT_FIELD_LEN-1 se truncaría al persistir. En un
// gestor de contraseñas, guardar una credencial truncada en silencio es
// peligroso: la entrada no serviría para iniciar sesión y el usuario no lo
// sabría. Devuelve true si el valor entra completo; false si se truncaría.
static bool _csv_field_fits(const String& v) {
    return v.length() <= (VAULT_FIELD_LEN - 1);
}

static bool _csv_notes_fits(const String& v) {
    return v.length() <= (VAULT_NOTES_LEN - 1);
}

// Extraer nombre limpio de un dominio: "account.deezer.com" → "Deezer"
static String _csv_clean_name(const char* raw) {
    String s = String(raw);
    s.trim();
    if (s.length() == 0) return s;

    // Si no parece un dominio (sin punto), devolver capitalizado
    if (s.indexOf('.') < 0) {
        s.setCharAt(0, toupper(s.charAt(0)));
        return s;
    }

    // Quitar protocolo
    if (s.startsWith("https://")) s = s.substring(8);
    if (s.startsWith("http://")) s = s.substring(7);
    if (s.startsWith("www.")) s = s.substring(4);

    // Tomar solo el host (hasta el primer /)
    int slash = s.indexOf('/');
    if (slash > 0) s = s.substring(0, slash);

    // Dividir por puntos y tomar el penúltimo segmento (dominio de segundo nivel)
    int last_dot = s.lastIndexOf('.');
    if (last_dot > 0) {
        String without_tld = s.substring(0, last_dot);
        int prev_dot = without_tld.lastIndexOf('.');
        if (prev_dot >= 0)
            s = without_tld.substring(prev_dot + 1);
        else
            s = without_tld;
    }

    // Capitalizar
    if (s.length() > 0) s.setCharAt(0, toupper(s.charAt(0)));
    return s;
}

// ── CSV: Exportar vault a archivo ─────────────────────────────
// Escribe el CSV línea por línea en el path dado (en SD)
// ── Línea CSV de una entrada (formato compartido con el export .hky) ──
static String _csv_export_line(const VaultEntry& e) {
    // Nombre de categoría
    String cat_name = "";
    if (strlen(e.category_id) > 0 && strcmp(e.category_id, "none") != 0) {
        VaultCategory cats[VAULT_MAX_CATS + 6];
        int cc = 0;
        vault_cat_list(cats, &cc, VAULT_MAX_CATS + 6);
        for (int j = 0; j < cc; j++) {
            if (strcmp(cats[j].id, e.category_id) == 0) { cat_name = String(cats[j].name); break; }
        }
    }

    auto escape = [](const char* val) -> String {
        String v = String(val);
        if (v.indexOf(',') >= 0 || v.indexOf('"') >= 0 || v.indexOf('\n') >= 0) {
            v.replace("\"", "\"\"");
            return "\"" + v + "\"";
        }
        return v;
    };

    // Saltos de línea aplanados: el lector de import es por líneas y un \n
    // dentro de comillas rompería la fila.
    String notes_flat = String(e.notes);
    notes_flat.replace("\r", " "); notes_flat.replace("\n", " ");

    if (e.type == VAULT_TYPE_WEB) {
        return escape(e.name) + ",web," +
               escape(e.username) + "," +
               escape(e.password) + ",," +
               escape(cat_name.c_str()) + "," +
               escape(notes_flat.c_str()) + "," +
               escape(e.totp_secret) + "\n";
    }
    return escape(e.name) + ",secret,,," +
           escape(e.secret) + "," +
           escape(cat_name.c_str()) + "," +
           escape(notes_flat.c_str()) + "," +
           escape(e.totp_secret) + "\n";
}

#define CSV_EXPORT_HEADER "nombre,tipo,usuario,contrasena,secreto,categoria,notas,totp\n"

// Retorna número de entradas exportadas o -1 si falla
int vault_export_csv(const char* path) {
    if (!_vault_ready) return -1;

    // Obtener lista de entradas (PSRAM: con miles de entradas no cabe en stack)
    char (*names)[VAULT_FIELD_LEN] = (char(*)[VAULT_FIELD_LEN])
        ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES_FS);
    if (!names) return -1;
    int count = 0;
    vault_list(names, &count, VAULT_MAX_ENTRIES_FS, CAT_ALL);
    if (count == 0) { free(names); return 0; }

    // Abrir archivo
    if (!sd_chunk_begin(path)) { free(names); return -1; }

    // Encabezado
    if (!sd_chunk_write((const uint8_t*)CSV_EXPORT_HEADER, strlen(CSV_EXPORT_HEADER))) {
        sd_chunk_abort(path); free(names); return -1;
    }

    int exported = 0;
    for (int i = 0; i < count; i++) {
        VaultEntry e = vault_load(names[i]);
        if (!e.valid) continue;
        String line = _csv_export_line(e);
        if (!sd_chunk_write((const uint8_t*)line.c_str(), line.length())) {
            sd_chunk_abort(path); free(names); return -1;
        }
        exported++;
    }

    sd_chunk_end();
    free(names);
    return exported;
}

// =============================================================
//  CSV: Exportación incremental (begin/step/progress)
//  Igual que el import incremental: unas pocas entradas por tick
//  desde un lv_timer para no congelar LVGL/touch, con progreso real.
// =============================================================
struct CsvExportState {
    bool active   = false;
    bool failed   = false;
    int  idx      = 0;
    int  count    = 0;
    int  exported = 0;
    char (*names)[VAULT_FIELD_LEN] = nullptr;
    char path[96] = {0};
};
static CsvExportState _cexp;

static void _csv_export_cleanup(bool abort_file) {
    if (abort_file) sd_chunk_abort(_cexp.path);
    if (_cexp.names) { free(_cexp.names); _cexp.names = nullptr; }
    _cexp.active = false;
    vault_op_busy_end();
}

// true = listo para vault_export_csv_step(); false = error
bool vault_export_csv_begin(const char* path) {
    if (!_vault_ready || _cexp.active) return false;
    vault_op_busy_begin();
    _cexp = CsvExportState();
    _cexp.active = true;
    strncpy(_cexp.path, path, sizeof(_cexp.path) - 1);

    _cexp.names = (char(*)[VAULT_FIELD_LEN])
        ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES_FS);
    if (!_cexp.names) { _cexp.failed = true; _csv_export_cleanup(false); return false; }
    vault_list(_cexp.names, &_cexp.count, VAULT_MAX_ENTRIES_FS, CAT_ALL);

    if (!sd_chunk_begin(path)) { _cexp.failed = true; _csv_export_cleanup(false); return false; }
    if (!sd_chunk_write((const uint8_t*)CSV_EXPORT_HEADER, strlen(CSV_EXPORT_HEADER))) {
        _cexp.failed = true; _csv_export_cleanup(true); return false;
    }
    return true;
}

// Exporta hasta `batch` entradas. true = queda trabajo; false = terminó
// (consultar vault_export_csv_result para éxito/fracaso).
bool vault_export_csv_step(int batch) {
    if (!_cexp.active) return false;
    for (int b = 0; b < batch && _cexp.idx < _cexp.count; b++, _cexp.idx++) {
        VaultEntry e = vault_load(_cexp.names[_cexp.idx]);
        if (!e.valid) continue;
        String line = _csv_export_line(e);
        if (!sd_chunk_write((const uint8_t*)line.c_str(), line.length())) {
            _cexp.failed = true;
            _csv_export_cleanup(true);
            return false;
        }
        _cexp.exported++;
    }
    if (_cexp.idx >= _cexp.count) {
        sd_chunk_end();
        _csv_export_cleanup(false);
        return false;
    }
    return true;
}

void vault_export_csv_progress(int* done, int* total, bool* active) {
    if (done)   *done   = _cexp.idx;
    if (total)  *total  = _cexp.count;
    if (active) *active = _cexp.active;
}

// Tras terminar: exportadas, o -1 si falló
int vault_export_csv_result() {
    return _cexp.failed ? -1 : _cexp.exported;
}

// ── CSV: Detectar formato del encabezado ──────────────────────
// Retorna 0=K-Hub, 1=Navegador (name/url/username/password), -1=desconocido
static int _csv_detect_format(const char* header) {
    String h = String(header);
    h.toLowerCase();
    h.replace("\t", ","); // normalizar tabs para búsqueda
    if (h.indexOf("tipo") >= 0) return 0;           // K-Hub
    if (h.indexOf("url") >= 0) return 1;             // Navegador
    if (h.indexOf("username") >= 0) return 1;        // Navegador sin url
    if (h.indexOf("usuario") >= 0) return 2;         // Plantilla K-Hub simple
    return -1;
}

// Mapear índices de columnas según el encabezado
struct CsvColMap {
    int nombre = -1, tipo = -1, usuario = -1;
    int password = -1, secreto = -1, categoria = -1;
    int url = -1, notas = -1, totp = -1;
};

static CsvColMap _csv_map_columns(char fields[][CSV_FIELD_LEN], int n, int format) {
    CsvColMap m;
    for (int i = 0; i < n; i++) {
        String f = String(fields[i]);
        f.toLowerCase(); f.trim();
        if (f == "nombre" || f == "name" || f == "title") m.nombre = i;
        else if (f == "tipo") m.tipo = i;
        else if (f == "usuario" || f == "username") m.usuario = i;
        else if (f == "contrasena" || f == "contraseña" || f == "password") m.password = i;
        else if (f == "secreto") m.secreto = i;
        else if (f == "categoria" || f == "category") m.categoria = i;
        else if (f == "notas" || f == "notes") m.notas = i;
        else if (f == "totp") m.totp = i;
        else if (f == "url") m.url = i;
    }
    return m;
}

// =============================================================
//  CSV: Importación incremental (begin/step/progress)
//  Para no bloquear el dispositivo entero durante import grandes —
//  se procesa unas pocas líneas por llamada desde un lv_timer en la
//  UI, dejando que LVGL/touch sigan respondiendo entre llamadas.
// =============================================================
struct CsvImportState {
    bool active      = false;
    int  imported    = 0;
    int  errors      = 0;
    int  lines_done  = 0;
    int  total_lines = 0; // estimado, sin contar encabezado
    CsvColMap col;
};
static CsvImportState _imp;

// ── Fuente de líneas pluggable ──────────────────────────────────
// Por defecto lee de SD (sd_readline_next/end). vault_hky.h la
// redirige a un lector que descifra sobre la marcha, para que el
// import de un .hky nunca materialice el CSV completo en texto
// plano en ningún almacenamiento — solo línea por línea, en RAM.
static bool (*_csv_src_next)(char*, int) = sd_readline_next;
static void (*_csv_src_end)()            = sd_readline_end;

// Abre el archivo, cuenta líneas totales (para el % de progreso) y
// detecta el formato/encabezado. Deja todo listo para vault_import_csv_step().
bool vault_import_csv_begin(const char* path) {
    if (!_vault_ready || _imp.active) return false;
    _csv_src_next = sd_readline_next;
    _csv_src_end  = sd_readline_end;

    // Pasada 1: solo contar líneas (rápido, no parsea nada)
    int total = 0;
    if (sd_readline_begin(path)) {
        static char count_buf[1024];
        while (sd_readline_next(count_buf, sizeof(count_buf))) total++;
        sd_readline_end();
    }

    // Pasada 2: abrir de nuevo y quedarnos posicionados después del encabezado
    if (!sd_readline_begin(path)) return false;
    static char header_buf[1024];
    if (!sd_readline_next(header_buf, sizeof(header_buf))) { sd_readline_end(); return false; }

    _csv_detect_sep(header_buf);
    int format = _csv_detect_format(header_buf);
    if (format < 0) { sd_readline_end(); return false; }

    char hfields[10][CSV_FIELD_LEN];
    int hcount = _csv_parse_line(header_buf, hfields, 10);
    _imp.col = _csv_map_columns(hfields, hcount, format);

    _imp.active      = true;
    _imp.imported    = 0;
    _imp.errors      = 0;
    _imp.lines_done  = 0;
    _imp.total_lines = total > 0 ? total - 1 : 0; // restar el encabezado ya contado
    return true;
}

// Variante para fuentes que no son un archivo SD reabrible — usada por
// vault_hky.h para alimentar este mismo parser desde un descifrado en
// streaming. El llamador ya conoce total_lines (lo cuenta en su propia
// primera pasada) y provee su propio lector de línea; aquí solo se
// consume el encabezado y se deja todo listo para vault_import_csv_step().
bool vault_import_csv_begin_streaming(bool (*next_fn)(char*, int), void (*end_fn)(), int total_lines) {
    if (!_vault_ready || _imp.active) return false;
    _csv_src_next = next_fn;
    _csv_src_end  = end_fn;

    static char header_buf[1024];
    if (!_csv_src_next(header_buf, sizeof(header_buf))) { _csv_src_end(); return false; }

    _csv_detect_sep(header_buf);
    int format = _csv_detect_format(header_buf);
    if (format < 0) { _csv_src_end(); return false; }

    char hfields[10][CSV_FIELD_LEN];
    int hcount = _csv_parse_line(header_buf, hfields, 10);
    _imp.col = _csv_map_columns(hfields, hcount, format);

    _imp.active      = true;
    _imp.imported    = 0;
    _imp.errors      = 0;
    _imp.lines_done  = 0;
    _imp.total_lines = total_lines;
    return true;
}

// Procesa hasta `batch` líneas más. Retorna true si quedan líneas
// pendientes (llamar otra vez), false si ya terminó (o no había nada activo).
bool vault_import_csv_step(int batch) {
    if (!_imp.active) return false;

    // Si el vault ya está lleno, no tiene sentido seguir: cada intento de
    // guardado recorrería toda la tabla hash (probing lineal + descifrado
    // por slot) para fallar igual. Terminar el import de forma ordenada;
    // las líneas restantes ya no se procesan.
    if (vault_count() >= VAULT_MAX_ENTRIES_FS) {
        _csv_src_end();
        _imp.active = false;
        return false;
    }

    static char line_buf[1024];
    int done_this_call = 0;

    while (done_this_call < batch) {
        if (!_csv_src_next(line_buf, sizeof(line_buf))) {
            _csv_src_end();
            _imp.active = false;
            return false; // terminado
        }
        _imp.lines_done++;
        done_this_call++;

        if (strlen(line_buf) == 0) continue;

        char fields[10][CSV_FIELD_LEN];
        int n = _csv_parse_line(line_buf, fields, 10);

        String nombre = "";
        if (_imp.col.nombre >= 0 && _imp.col.nombre < n) {
            String raw = String(fields[_imp.col.nombre]);
            if (raw.indexOf(".") > 0 && raw.indexOf(" ") < 0)
                nombre = _csv_clean_name(fields[_imp.col.nombre]);
            else
                nombre = raw;
        } else if (_imp.col.url >= 0 && _imp.col.url < n)
            nombre = _csv_clean_name(fields[_imp.col.url]);
        if (nombre.length() == 0) { _imp.errors++; continue; }

        String tipo = "web";
        if (_imp.col.tipo >= 0 && _imp.col.tipo < n)
            tipo = String(fields[_imp.col.tipo]);

        String usuario   = (_imp.col.usuario   >= 0 && _imp.col.usuario   < n) ? String(fields[_imp.col.usuario])   : "";
        String password  = (_imp.col.password  >= 0 && _imp.col.password  < n) ? String(fields[_imp.col.password])  : "";
        String secreto   = (_imp.col.secreto   >= 0 && _imp.col.secreto   < n) ? String(fields[_imp.col.secreto])   : "";
        String categoria = (_imp.col.categoria >= 0 && _imp.col.categoria < n) ? String(fields[_imp.col.categoria]) : "";
        String notas     = (_imp.col.notas     >= 0 && _imp.col.notas     < n) ? String(fields[_imp.col.notas])     : "";
        String totp      = (_imp.col.totp      >= 0 && _imp.col.totp      < n) ? String(fields[_imp.col.totp])      : "";

        String cat_id = "none";
        if (categoria.length() > 0) {
            VaultCategory cats[VAULT_MAX_CATS + 6];
            int cc = 0;
            vault_cat_list(cats, &cc, VAULT_MAX_CATS + 6);
            for (int j = 0; j < cc; j++) {
                if (String(cats[j].name).equalsIgnoreCase(categoria)) { cat_id = String(cats[j].id); break; }
            }
            if (cat_id == "none") {
                vault_cat_create(categoria.c_str());
                vault_cat_list(cats, &cc, VAULT_MAX_CATS + 6);
                for (int j = 0; j < cc; j++) {
                    if (String(cats[j].name).equalsIgnoreCase(categoria)) { cat_id = String(cats[j].id); break; }
                }
            }
        }

        bool ok = false;
        // Si un campo sensible se truncaría, no guardar una credencial parcial:
        // contarla como error para que el usuario la corrija en origen.
        if (!_csv_field_fits(nombre) || !_csv_field_fits(usuario) ||
            !_csv_field_fits(password) || !_csv_field_fits(secreto) ||
            !_csv_notes_fits(notas)) {
            _imp.errors++;
            continue;
        }
        // "secret" con solo nota (secreto vacío) también es válido — entradas
        // creadas con el toggle "Guardar solo nota".
        if (tipo == "secret" && (secreto.length() > 0 || notas.length() > 0)) {
            ok = vault_save_secret(nombre.c_str(), secreto.c_str(), cat_id.c_str(), notas.c_str());
        } else if (usuario.length() > 0 || password.length() > 0) {
            ok = vault_save_web(nombre.c_str(), usuario.c_str(), password.c_str(), cat_id.c_str(), notas.c_str());
        }
        // Restaurar secreto TOTP si está presente en el registro
        if (ok && totp.length() > 0 && _csv_field_fits(totp))
            vault_set_totp_secret(nombre.c_str(), totp.c_str());

        if (ok) _imp.imported++;
        else _imp.errors++;
    }
    return true; // se acabó el batch, pero todavía puede haber más
}

void vault_import_csv_cancel() {
    if (_imp.active) { _csv_src_end(); _imp.active = false; }
}

void vault_import_csv_progress(int* imported, int* errors, int* lines_done, int* total_lines, bool* active) {
    if (imported)    *imported    = _imp.imported;
    if (errors)      *errors      = _imp.errors;
    if (lines_done)  *lines_done  = _imp.lines_done;
    if (total_lines) *total_lines = _imp.total_lines;
    if (active)      *active      = _imp.active;
}

// Retorna número de entradas importadas o -1 si falla
// on_progress(imported, total_lines) — callback opcional de progreso
int vault_import_csv(const char* path, int* errors_out) {
    if (!_vault_ready) return -1;
    if (errors_out) *errors_out = 0;

    if (!sd_readline_begin(path)) return -1;

    // Leer encabezado
    static char header_buf[1024];
    if (!sd_readline_next(header_buf, sizeof(header_buf))) {
        sd_readline_end(); return -1;
    }

    _csv_detect_sep(header_buf); // Detectar separador: coma o tabulador
    int format = _csv_detect_format(header_buf);
    if (format < 0) { sd_readline_end(); return -1; }

    char hfields[10][CSV_FIELD_LEN];
    int hcount = _csv_parse_line(header_buf, hfields, 10);
    CsvColMap col = _csv_map_columns(hfields, hcount, format);

    int imported = 0;
    int errors = 0;
    static char line_buf[1024];

    while (sd_readline_next(line_buf, sizeof(line_buf))) {
        if (strlen(line_buf) == 0) continue;

        char fields[10][CSV_FIELD_LEN];
        int n = _csv_parse_line(line_buf, fields, 10);

        // Extraer nombre
        String nombre = "";
        if (col.nombre >= 0 && col.nombre < n) {
            // Formato plantilla: usar nombre directo sin limpiar
            // Formato navegador con URL en nombre: limpiar dominio
            String raw = String(fields[col.nombre]);
            if (raw.indexOf(".") > 0 && raw.indexOf(" ") < 0)
                nombre = _csv_clean_name(fields[col.nombre]);
            else
                nombre = raw;
        } else if (col.url >= 0 && col.url < n)
            nombre = _csv_clean_name(fields[col.url]);
        if (nombre.length() == 0) { errors++; continue; }

        // Extraer tipo
        String tipo = "web"; // formato plantilla y navegador siempre web
        if (col.tipo >= 0 && col.tipo < n)
            tipo = String(fields[col.tipo]);

        // Extraer campos
        String usuario = (col.usuario >= 0 && col.usuario < n) ? String(fields[col.usuario]) : "";
        String password = (col.password >= 0 && col.password < n) ? String(fields[col.password]) : "";
        String secreto = (col.secreto >= 0 && col.secreto < n) ? String(fields[col.secreto]) : "";
        String categoria = (col.categoria >= 0 && col.categoria < n) ? String(fields[col.categoria]) : "";
        String notas = (col.notas >= 0 && col.notas < n) ? String(fields[col.notas]) : "";
        String totp = (col.totp >= 0 && col.totp < n) ? String(fields[col.totp]) : "";

        // Buscar o crear categoría
        String cat_id = "none";
        if (categoria.length() > 0) {
            VaultCategory cats[VAULT_MAX_CATS + 6];
            int cc = 0;
            vault_cat_list(cats, &cc, VAULT_MAX_CATS + 6);
            for (int j = 0; j < cc; j++) {
                if (String(cats[j].name).equalsIgnoreCase(categoria)) {
                    cat_id = String(cats[j].id);
                    break;
                }
            }
            if (cat_id == "none") {
                // Crear categoría nueva
                vault_cat_create(categoria.c_str());
                // Buscar el ID recién creado
                vault_cat_list(cats, &cc, VAULT_MAX_CATS + 6);
                for (int j = 0; j < cc; j++) {
                    if (String(cats[j].name).equalsIgnoreCase(categoria)) {
                        cat_id = String(cats[j].id);
                        break;
                    }
                }
            }
        }

        // Guardar entrada
        bool ok = false;
        if (!_csv_field_fits(nombre) || !_csv_field_fits(usuario) ||
            !_csv_field_fits(password) || !_csv_field_fits(secreto) ||
            !_csv_notes_fits(notas)) {
            errors++;
            continue;
        }
        // "secret" con solo nota también es válido (toggle "Guardar solo nota")
        if (tipo == "secret" && (secreto.length() > 0 || notas.length() > 0)) {
            ok = vault_save_secret(nombre.c_str(), secreto.c_str(), cat_id.c_str(), notas.c_str());
        } else if (usuario.length() > 0 || password.length() > 0) {
            ok = vault_save_web(nombre.c_str(), usuario.c_str(),
                                password.c_str(), cat_id.c_str(), notas.c_str());
        }
        if (ok && totp.length() > 0 && _csv_field_fits(totp))
            vault_set_totp_secret(nombre.c_str(), totp.c_str());

        if (ok) imported++;
        else errors++;
    }

    sd_readline_end();
    if (errors_out) *errors_out = errors;
    return imported;
}
