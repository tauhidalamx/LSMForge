import type { Metadata } from 'next';
import './globals.css';
import { Sidebar } from '@/components/Sidebar';

export const metadata: Metadata = {
  title: 'LSMForge — High-Performance C++20 LSM-Tree Database',
  description:
    'Full-stack LSM-Tree key-value database dashboard, operational monitoring, compaction visualizer, benchmark engine, and query console.',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en" className="dark h-full bg-[#08090d] text-slate-100 antialiased">
      <body className="h-full flex overflow-hidden bg-[#08090d]">
        <Sidebar />
        <div className="flex-1 flex flex-col min-w-0 h-screen overflow-y-auto bg-[#08090d]">
          {children}
        </div>
      </body>
    </html>
  );
}
