from pathlib import Path
p = Path(r"C:\Users\doge6\Videos\GammaSource-main\cl_dll\ragdoll.cpp")
d = p.read_bytes()
nl = b"\r\n" if b"\r\n" in d else b"\n"
old = (b"\t\t\tif ( !pCollide )" + nl +
       b"\t\t\t\tError( \"C_ServerRagdoll::InitModel: missing vcollide data\" );" + nl + nl +
       b"\t\t\tm_elementCount = RagdollExtractBoneIndices( m_boneIndex, hdr, pCollide );")
new = (b"\t\t\t// A model without collision data (e.g. an installed later-format model) used to" + nl +
       b"\t\t\t// abort the whole engine; draw it unposed instead." + nl +
       b"\t\t\tif ( !pCollide )" + nl +
       b"\t\t\t{" + nl +
       b"\t\t\t\tWarning( \"C_ServerRagdoll::InitModel: missing vcollide data\\n\" );" + nl +
       b"\t\t\t\treturn hdr;" + nl +
       b"\t\t\t}" + nl + nl +
       b"\t\t\tm_elementCount = RagdollExtractBoneIndices( m_boneIndex, hdr, pCollide );")
assert d.count(old) == 1, "pattern"
p.write_bytes(d.replace(old, new))
print("ok")
