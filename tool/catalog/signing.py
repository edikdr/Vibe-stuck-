# -*- coding: utf-8 -*-
"""Ed25519 signing of catalog releases.

The client already refuses a download whose SHA-256 does not match the
manifest, and HTTPS to the forge authenticates the manifest itself. A
signature adds the one thing those cannot: it survives the forge. Whoever can
publish a release — a compromised token, a mistaken maintainer, the host
itself — cannot produce a manifest the app accepts without the private key.

What is signed is not the manifest file but an explicit payload built from the
fields that decide what gets installed. Signing the serialized JSON would mean
the client had to reproduce it byte for byte, and the first added field would
break every installed app. This payload is built from named values instead, so
the manifest can grow without invalidating anything.
"""
from __future__ import annotations

import base64

PAYLOAD_VERSION = 'vibestack-atlas-catalog-v1'
SIGNATURE_PREFIX = 'ed25519:'


def signing_payload(manifest: dict) -> bytes:
    """The exact bytes a signature covers.

    Kept identical to `CatalogManifest.signingPayload` in Dart. Every field
    here decides either what is downloaded or whether it is installed at all.
    """
    lines = [
        PAYLOAD_VERSION,
        str(manifest['schemaVersion']),
        str(manifest['catalogVersion']),
        str(manifest['size']),
        str(manifest['sha256']),
        str(manifest['databaseSize']),
        str(manifest['databaseSha256']),
    ]
    return '\n'.join(lines).encode('utf-8')


def generate_keypair() -> tuple[str, str]:
    """A new (private seed, public key) pair, both base64. Private stays secret."""
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives import serialization

    private = Ed25519PrivateKey.generate()
    seed = private.private_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PrivateFormat.Raw,
        encryption_algorithm=serialization.NoEncryption(),
    )
    public = private.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    )
    return base64.b64encode(seed).decode(), base64.b64encode(public).decode()


def public_key_of(private_key_b64: str) -> str:
    """The public key belonging to a private seed, so the two cannot drift."""
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives import serialization

    private = Ed25519PrivateKey.from_private_bytes(_decode(private_key_b64, 32, 'private key'))
    public = private.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    )
    return base64.b64encode(public).decode()


def sign(manifest: dict, private_key_b64: str) -> str:
    """Signature of `manifest`, as the `ed25519:<base64>` the client expects."""
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

    private = Ed25519PrivateKey.from_private_bytes(_decode(private_key_b64, 32, 'private key'))
    signature = private.sign(signing_payload(manifest))
    return SIGNATURE_PREFIX + base64.b64encode(signature).decode()


def verify(manifest: dict, signature: str, public_key_b64: str) -> bool:
    """Whether `signature` was made over `manifest` by `public_key_b64`.

    Used by the tests and by the release job to check its own output before
    publishing it — a signature nobody verified is not evidence of anything.
    """
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
    from cryptography.exceptions import InvalidSignature

    if not signature.startswith(SIGNATURE_PREFIX):
        return False
    try:
        raw = base64.b64decode(signature[len(SIGNATURE_PREFIX):], validate=True)
        public = Ed25519PublicKey.from_public_bytes(_decode(public_key_b64, 32, 'public key'))
        public.verify(raw, signing_payload(manifest))
    except (InvalidSignature, ValueError):
        return False
    return True


def _decode(value: str, length: int, what: str) -> bytes:
    raw = base64.b64decode(value.strip(), validate=True)
    if len(raw) != length:
        raise ValueError(f'{what} must be {length} bytes, got {len(raw)}')
    return raw
