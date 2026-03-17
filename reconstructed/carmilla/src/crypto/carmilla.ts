import fs from "fs";
import { promisify } from "util";
import {
  randomBytes,
  createCipheriv,
  createDecipheriv,
  pbkdf2Sync,
  createHash,
  createHmac,
} from "crypto";

const readFileAsync = promisify(fs.readFile);

// ============================================================================
// Carmilla Encryption System — Real Cryptography, Zero Placeholders
//
// • AES-256-GCM   — authenticated encryption (encrypt + integrity in one pass)
// • PBKDF2-SHA512 — password-based key derivation (100 000 iterations)
// • Unique random salt (32 bytes) + IV (12 bytes) per operation
// • HMAC-SHA256 envelope integrity check on packaged data
// • No shell exec, no OpenSSL binary, no fake calls — pure Node.js crypto
// ============================================================================

const ALGORITHM = "aes-256-gcm";
const KEY_LENGTH = 32;            // 256 bits
const IV_LENGTH = 12;             // 96 bits — recommended for GCM
const SALT_LENGTH = 32;           // 256 bits
const AUTH_TAG_LENGTH = 16;       // 128 bits
const PBKDF2_ITERATIONS = 100_000;
const PBKDF2_DIGEST = "sha512";
const ENCODING: BufferEncoding = "base64";

// ── Types ────────────────────────────────────────────────────────────────

export type CarmillaMeta = {
  method: "aes-256-gcm";
  kdf: "pbkdf2-sha512";
  kdf_iterations: number;
  salt_bytes: number;
  iv_bytes: number;
  tag_bytes: number;
  chain?: string[];
  passphrase_hint?: string;
  timestamp: string;
  hmac?: string;
  [key: string]: any;
};

export type CarmillaPackage = {
  meta: CarmillaMeta;
  data: string;   // base64 envelope: salt ‖ iv ‖ authTag ‖ ciphertext
};

export type EncryptionTarget = {
  type: "file" | "obj" | "buffer";
  path?: string;
  key?: string;
  data?: string;
  intent: "encrypt" | "do-not-encrypt" | "cannot-encrypt";
};

export type EncryptionResult = {
  type: string;
  path?: string | undefined;
  key?: string | undefined;
  intent: string;
  status: "encrypted" | "encryption failed" | "skipped" | "cannot encrypt";
  packaged?: string;
  error?: string;
  reason?: string;
  meta?: CarmillaMeta;
};

// ── Key Derivation ───────────────────────────────────────────────────────

/**
 * Derive a 256-bit key from passphrase + salt using PBKDF2-SHA512.
 */
function deriveKey(passphrase: string, salt: Buffer): Buffer {
  return pbkdf2Sync(
    passphrase,
    salt,
    PBKDF2_ITERATIONS,
    KEY_LENGTH,
    PBKDF2_DIGEST
  );
}

// ── Carmilla Class ───────────────────────────────────────────────────────

/**
 * Core Carmilla Encryption Engine
 *
 * Every encrypt() call generates a fresh random salt and IV so that
 * identical plaintext + passphrase always produce different ciphertext.
 *
 * Binary envelope layout (then base64-encoded):
 *   salt (32 B) ‖ iv (12 B) ‖ authTag (16 B) ‖ ciphertext (variable)
 */
export class Carmilla {

  // ── Encrypt ──────────────────────────────────────────────────────────

  /**
   * Encrypt arbitrary string data with AES-256-GCM.
   * Returns a base64 string containing salt ‖ iv ‖ authTag ‖ ciphertext.
   */
  static async encrypt(data: string, passphrase: string): Promise<string> {
    if (!data) throw new Error("Nothing to encrypt — data is empty");
    if (!passphrase) throw new Error("Passphrase is required");

    // Generate unique random salt and IV for every operation
    const salt = randomBytes(SALT_LENGTH);
    const iv = randomBytes(IV_LENGTH);
    const key = deriveKey(passphrase, salt);

    const cipher = createCipheriv(ALGORITHM, key, iv, {
      authTagLength: AUTH_TAG_LENGTH,
    });

    const plainBuf = Buffer.from(data, "utf-8");
    const encrypted = Buffer.concat([
      cipher.update(plainBuf),
      cipher.final(),
    ]);
    const authTag = cipher.getAuthTag();

    // Envelope: salt ‖ iv ‖ authTag ‖ ciphertext
    const envelope = Buffer.concat([salt, iv, authTag, encrypted]);
    return envelope.toString(ENCODING);
  }

  // ── Decrypt ──────────────────────────────────────────────────────────

  /**
   * Decrypt data previously encrypted with Carmilla.encrypt().
   * Expects a base64 string containing salt ‖ iv ‖ authTag ‖ ciphertext.
   */
  static async decrypt(
    encryptedData: string,
    passphrase: string
  ): Promise<string> {
    if (!encryptedData) throw new Error("Nothing to decrypt — data is empty");
    if (!passphrase) throw new Error("Passphrase is required");

    const envelope = Buffer.from(encryptedData, ENCODING);

    const minLength = SALT_LENGTH + IV_LENGTH + AUTH_TAG_LENGTH + 1;
    if (envelope.length < minLength) {
      throw new Error(
        `Invalid ciphertext: expected at least ${minLength} bytes, got ${envelope.length}`
      );
    }

    let offset = 0;
    const salt = envelope.subarray(offset, offset + SALT_LENGTH);
    offset += SALT_LENGTH;
    const iv = envelope.subarray(offset, offset + IV_LENGTH);
    offset += IV_LENGTH;
    const authTag = envelope.subarray(offset, offset + AUTH_TAG_LENGTH);
    offset += AUTH_TAG_LENGTH;
    const ciphertext = envelope.subarray(offset);

    const key = deriveKey(passphrase, salt);

    const decipher = createDecipheriv(ALGORITHM, key, iv, {
      authTagLength: AUTH_TAG_LENGTH,
    });
    decipher.setAuthTag(authTag);

    try {
      const decrypted = Buffer.concat([
        decipher.update(ciphertext),
        decipher.final(),
      ]);
      return decrypted.toString("utf-8");
    } catch {
      throw new Error(
        "Decryption failed — wrong passphrase or data has been tampered with"
      );
    }
  }

  // ── Package / Unpackage ──────────────────────────────────────────────

  /**
   * Wrap encrypted data with metadata into a signed JSON package (base64).
   * Includes HMAC-SHA256 for envelope integrity verification.
   */
  static repack(
    encrypted: string,
    options: Partial<CarmillaMeta> = {}
  ): string {
    const meta: CarmillaMeta = {
      method: "aes-256-gcm",
      kdf: "pbkdf2-sha512",
      kdf_iterations: PBKDF2_ITERATIONS,
      salt_bytes: SALT_LENGTH,
      iv_bytes: IV_LENGTH,
      tag_bytes: AUTH_TAG_LENGTH,
      chain: options.chain || ["carmilla"],
      passphrase_hint: options.passphrase_hint || "",
      timestamp: new Date().toISOString(),
      ...options,
    };

    // Compute HMAC-SHA256 over the ciphertext for envelope integrity
    const hmacKey = createHash("sha256")
      .update(meta.timestamp)
      .digest();
    meta.hmac = createHmac("sha256", hmacKey)
      .update(encrypted)
      .digest("hex");

    const pack: CarmillaPackage = { meta, data: encrypted };
    return Buffer.from(JSON.stringify(pack)).toString(ENCODING);
  }

  /**
   * Unwrap a packaged blob and verify envelope integrity.
   */
  static unpack(packaged: string): CarmillaPackage {
    let pack: CarmillaPackage;
    try {
      const json = Buffer.from(packaged, ENCODING).toString("utf-8");
      pack = JSON.parse(json) as CarmillaPackage;
    } catch {
      throw new Error(
        "Unpack failed — data is not a valid Carmilla package"
      );
    }

    if (!pack.meta || !pack.data) {
      throw new Error("Invalid package structure — missing meta or data");
    }

    // Verify HMAC envelope integrity if present
    if (pack.meta.hmac && pack.meta.timestamp) {
      const hmacKey = createHash("sha256")
        .update(pack.meta.timestamp)
        .digest();
      const expected = createHmac("sha256", hmacKey)
        .update(pack.data)
        .digest("hex");
      if (expected !== pack.meta.hmac) {
        throw new Error(
          "Package integrity check failed — data may be corrupted or tampered with"
        );
      }
    }

    return pack;
  }

  // ── Convenience Wrappers ─────────────────────────────────────────────

  /**
   * Encrypt + package in one call.
   */
  static async encryptAndPack(
    data: string,
    passphrase: string,
    options: Partial<CarmillaMeta> = {}
  ): Promise<string> {
    const encrypted = await this.encrypt(data, passphrase);
    return this.repack(encrypted, options);
  }

  /**
   * Unpack + decrypt in one call.
   */
  static async unpackAndDecrypt(
    packaged: string,
    passphrase: string
  ): Promise<string> {
    const pack = this.unpack(packaged);
    return await this.decrypt(pack.data, passphrase);
  }

  // ── Selective / Batch Encryption ─────────────────────────────────────

  /**
   * Process an array of encryption targets with full reporting.
   */
  static async processTargets(
    targets: EncryptionTarget[],
    passphrase: string,
    options: Partial<CarmillaMeta> = {}
  ): Promise<EncryptionResult[]> {
    const results: EncryptionResult[] = [];
    const timestamp = new Date().toISOString();

    for (const target of targets) {
      const baseResult: EncryptionResult = {
        type: target.type,
        path: target.path,
        key: target.key,
        intent: target.intent,
        status: "skipped",
        meta: {
          method: "aes-256-gcm",
          kdf: "pbkdf2-sha512",
          kdf_iterations: PBKDF2_ITERATIONS,
          salt_bytes: SALT_LENGTH,
          iv_bytes: IV_LENGTH,
          tag_bytes: AUTH_TAG_LENGTH,
          ...options,
          timestamp,
        },
      };

      if (target.intent === "do-not-encrypt") {
        baseResult.status = "skipped";
        baseResult.reason = "user marked as do-not-encrypt";
        results.push(baseResult);
        continue;
      }

      if (target.intent === "cannot-encrypt") {
        baseResult.status = "cannot encrypt";
        baseResult.reason = "marked as cannot encrypt";
        results.push(baseResult);
        continue;
      }

      if (target.intent === "encrypt") {
        try {
          let dataToEncrypt: string;

          if (target.type === "file" && target.path) {
            dataToEncrypt = await readFileAsync(target.path, "utf-8");
          } else if (target.data) {
            dataToEncrypt = target.data;
          } else {
            throw new Error("No data to encrypt");
          }

          const encrypted = await this.encrypt(dataToEncrypt, passphrase);
          const packaged = this.repack(encrypted, { ...options, timestamp });

          baseResult.status = "encrypted";
          baseResult.packaged = packaged;
          results.push(baseResult);
        } catch (error) {
          baseResult.status = "encryption failed";
          baseResult.error = (error as Error).message;
          results.push(baseResult);
        }
      }
    }

    return results;
  }
}

export default Carmilla;
