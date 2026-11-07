import { initializeApp } from "https://www.gstatic.com/firebasejs/10.7.1/firebase-app.js";
import { getDatabase, ref, set, get, onValue, push, remove, child } from "https://www.gstatic.com/firebasejs/10.7.1/firebase-database.js";

// ==========================================================
// KONFIGURASI FIREBASE
// ==========================================================
const firebaseConfig = {
    // SILA TUKAR PAUTAN DI BAWAH JIKA IA BERBEZA DENGAN FIREBASE ANDA
    databaseURL: "https://lesson-10---rfid-default-rtdb.asia-southeast1.firebasedatabase.app" 
};

const app = initializeApp(firebaseConfig);
const db = getDatabase(app);

// Rujukan Database
const pelajarRef = ref(db, 'pelajar');
const logRef = ref(db, 'log_kehadiran');
const kadTerkiniRef = ref(db, 'kad_terkini');
const whitelistRef = ref(db, 'whitelist');

let profilPelajar = {};
let jumlahPelajar = 0;
const HAD_PELAJAR = 7;

// ==========================================================
// 1. AUTO-FILL UID KAD TERKINI (Untuk borang pendaftaran)
// ==========================================================
onValue(kadTerkiniRef, (snapshot) => {
    if (snapshot.exists()) {
        document.getElementById('uid').value = snapshot.val();
    }
});

// ==========================================================
// 2. PAPAR SENARAI PELAJAR
// ==========================================================
onValue(pelajarRef, (snapshot) => {
    const tbody = document.getElementById('jadual-pelajar');
    tbody.innerHTML = ''; 
    profilPelajar = {}; 
    jumlahPelajar = 0;

    if (snapshot.exists()) {
        const data = snapshot.val();
        for (let key in data) {
            jumlahPelajar++;
            const pelajar = data[key];
            
            // Buang ruang kosong pada UID untuk dijadikan kunci (key) rujukan
            const uidBersih = pelajar.uid.replace(/\s/g, '').toUpperCase();
            profilPelajar[uidBersih] = pelajar; 

            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td>${pelajar.nama}</td>
                <td>${pelajar.kelas}</td>
                <td>${pelajar.noid}</td>
                <td>${pelajar.uid}</td> 
                <td>
                    <button class="btn-padam" onclick="padamPelajar('${key}', '${uidBersih}')">Padam</button>
                </td>
            `;
            tbody.appendChild(tr);
        }
    } else {
        // Jika tiada rekod, paparkan mesej ini merentasi 5 lajur
        tbody.innerHTML = `<tr><td colspan="5" style="text-align:center;">Tiada rekod pelajar.</td></tr>`;
    }
});

// ==========================================================
// 3. TAMBAH PELAJAR BAHARU
// ==========================================================
document.getElementById('btn-simpan').addEventListener('click', () => {
    if (jumlahPelajar >= HAD_PELAJAR) {
        alert("Maaf, kuota pelajar telah penuh (Had 7 orang).");
        return;
    }

    const nama = document.getElementById('nama').value.toUpperCase();
    const kelas = document.getElementById('kelas').value.toUpperCase();
    const noid = document.getElementById('noid').value.toUpperCase();
    const uid = document.getElementById('uid').value.toUpperCase();

    if (!nama || !kelas || !noid || !uid) {
        alert("Sila isi semua maklumat!");
        return;
    }

    const uidBersih = uid.replace(/\s/g, '');

    // Semak jika UID sudah wujud dalam senarai putih (whitelist)
    get(child(ref(db), `whitelist/${uidBersih}`)).then((snapshot) => {
        if (snapshot.exists() && snapshot.val() === true) {
            alert("Ralat: UID Kad ini telah didaftarkan untuk pelajar lain!");
        } else {
            // Simpan data ke nod 'pelajar'
            const newPelajarRef = push(pelajarRef);
            set(newPelajarRef, {
                nama: nama,
                kelas: kelas,
                noid: noid,
                uid: uid
            });

            // Benarkan akses dengan menyimpan UID ke nod 'whitelist'
            set(ref(db, `whitelist/${uidBersih}`), true);

            alert("Maklumat pelajar berjaya disimpan!");
            
            // Kosongkan borang
            document.getElementById('nama').value = '';
            document.getElementById('kelas').value = '';
            document.getElementById('noid').value = '';
            document.getElementById('uid').value = '';
            
            // Padam rekod kad_terkini supaya input auto-fill kosong semula
            remove(kadTerkiniRef);
        }
    });
});

// ==========================================================
// 4. PADAM PELAJAR (Fungsi Global untuk butang HTML)
// ==========================================================
window.padamPelajar = function(key, uidBersih) {
    if (confirm("Adakah anda pasti mahu memadam rekod pelajar ini?")) {
        // Buang data dari jadual pelajar
        remove(ref(db, `pelajar/${key}`));
        // Tarik balik akses dengan membuang data dari whitelist
        remove(ref(db, `whitelist/${uidBersih}`));
    }
};

// ==========================================================
// 5. PAPAR LOG KEHADIRAN
// ==========================================================
onValue(logRef, (snapshot) => {
    const tbodyLog = document.getElementById('jadual-log');
    tbodyLog.innerHTML = '';

    if (snapshot.exists()) {
        const data = snapshot.val();
        
        // Tukar objek data kepada array
        let senaraiLog = Object.values(data);
        
        // Susun array supaya masa (timestamp) terbaharu berada di paling atas
        senaraiLog.sort((a, b) => (b.timestamp || 0) - (a.timestamp || 0));

        senaraiLog.forEach((log) => {
            const logUIDBersih = log.uid ? log.uid.replace(/\s/g, '').toUpperCase() : "";
            let namaPelajar = "Pelajar Tidak Dikenali";
            let kelasPelajar = "-";

            // Padankan UID log dengan maklumat dalam profilPelajar
            if (profilPelajar[logUIDBersih]) {
                namaPelajar = profilPelajar[logUIDBersih].nama;
                kelasPelajar = profilPelajar[logUIDBersih].kelas;
            }

            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td>${log.tarikh || '-'}</td>
                <td>${log.masaMasuk || '-'}</td>
                <td>${log.masaKeluar || '-'}</td>
                <td><strong>${namaPelajar}</strong></td>
                <td>${kelasPelajar}</td>
                <td>${log.uid || '-'}</td>
            `;
            tbodyLog.appendChild(tr);
        });
    } else {
        tbodyLog.innerHTML = `<tr><td colspan="6" style="text-align:center;">Tiada rekod kehadiran.</td></tr>`;
    }
});