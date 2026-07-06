import { useEffect, useMemo, useRef } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls } from '@react-three/drei';
import * as THREE from 'three';
import { CUBE_N, NUM_LEDS, type Layout } from '@shared/types';
import { ledPosition, makeIndex } from '@shared/geometry';

// Preview iterates physical (x,y,z) and uses the wiring layout to look up colors.
// Orientation is applied server-side when filling the buffer, so the preview
// already shows the correctly-rotated physical cube.

type UpAxis = 'x' | 'y' | 'z';

interface Props {
  rgb: Uint8Array | null;
  layout: Layout;
  background: string;
  up: UpAxis;
}

export function CubePreview({ rgb, layout, background, up }: Props) {
  // Pick a wireframe color that contrasts with the background so the cube
  // outline stays visible across light/dark themes.
  const wireColor = pickContrastingWire(background);
  return (
    <Canvas camera={{ position: [1.6, 1.4, 2.0], fov: 45 }} style={{ height: '100%', background }}>
      <ambientLight intensity={0.4} />
      <Pixels rgb={rgb} layout={layout} up={up} />
      <CubeFrame color={wireColor} />
      <OrbitControls makeDefault enablePan={false} />
    </Canvas>
  );
}

// Remap a centered (x, y, z) physical-cube position into Three.js screen
// coords (where +Y is the screen-up axis), so that the user-chosen axis
// shows as vertical on screen. Display-only — does not touch the buffer.
function applyUp(pos: [number, number, number], up: UpAxis): [number, number, number] {
  const [x, y, z] = pos;
  if (up === 'z') return [x, z, y];
  if (up === 'x') return [y, x, z];
  return [x, y, z];
}

function CubeFrame({ color }: { color: string }) {
  return (
    <lineSegments>
      <edgesGeometry args={[new THREE.BoxGeometry(1.05, 1.05, 1.05)]} />
      <lineBasicMaterial color={color} />
    </lineSegments>
  );
}

function pickContrastingWire(bg: string): string {
  // Sample the background luminance and pick a soft accent on either side.
  const c = new THREE.Color(bg);
  const lum = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
  return lum > 0.5 ? '#888' : '#222';
}

function Pixels({ rgb, layout, up }: { rgb: Uint8Array | null; layout: Layout; up: UpAxis }) {
  const meshRef = useRef<THREE.InstancedMesh>(null);
  const tmp = useMemo(() => new THREE.Object3D(), []);
  const color = useMemo(() => new THREE.Color(), []);
  const idx = useMemo(() => makeIndex(layout), [layout]);

  // Re-initialize instance positions whenever the up-axis changes so the
  // visible orientation flips even though the buffer stays the same.
  useEffect(() => {
    const mesh = meshRef.current;
    if (!mesh) return;
    let i = 0;
    for (let x = 0; x < CUBE_N; x++) {
      for (let y = 0; y < CUBE_N; y++) {
        for (let z = 0; z < CUBE_N; z++) {
          const [px, py, pz] = applyUp(ledPosition(x, y, z), up);
          tmp.position.set(px, py, pz);
          tmp.updateMatrix();
          mesh.setMatrixAt(i++, tmp.matrix);
        }
      }
    }
    mesh.instanceMatrix.needsUpdate = true;
  }, [tmp, up]);

  useFrame(() => {
    const mesh = meshRef.current;
    if (!mesh || !rgb) return;
    let i = 0;
    for (let x = 0; x < CUBE_N; x++) {
      for (let y = 0; y < CUBE_N; y++) {
        for (let z = 0; z < CUBE_N; z++) {
          const led = idx(x, y, z) * 3;
          const r = rgb[led] / 255;
          const g = rgb[led + 1] / 255;
          const b = rgb[led + 2] / 255;
          color.setRGB(r, g, b);
          mesh.setColorAt(i, color);
          i++;
        }
      }
    }
    if (mesh.instanceColor) mesh.instanceColor.needsUpdate = true;
  });

  return (
    <instancedMesh ref={meshRef} args={[undefined as any, undefined as any, NUM_LEDS]}>
      <sphereGeometry args={[0.018, 8, 8]} />
      <meshBasicMaterial toneMapped={false} />
    </instancedMesh>
  );
}
