#[derive(Debug, PartialEq, Hash, Eq, Clone)]
pub struct PackageArch {
    pub package: Package,
    pub architecture: Architecture,
}
type Package = String;
type Architecture = String;
